#include "parser/parser.hpp"

#include "debug.hpp"
#include "lexer/lexer.hpp"
#include "profiler.hpp"
#include "resolver/resolver.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

std::map<unsigned int, CallNodeBase *> Parser::call_nodes;
std::mutex Parser::call_nodes_mutex;

FileNode Parser::parse() {
    std::string file_name = file.filename();
    PROFILE_SCOPE("Parsing file '" + file_name + "'");
    FileNode file_node(file_name);
    token_list tokens = Lexer(file).scan();
    Debug::print_token_context_vector(tokens);
    // Consume all tokens and convert them to nodes
    while (!tokens.empty()) {
        add_next_main_node(file_node, tokens);
    }
    return file_node;
}

std::optional<CallNodeBase *> Parser::get_call_from_id(unsigned int call_id) {
    // The mutex will unlock by itself when it goes out of scope
    std::lock_guard<std::mutex> lock(call_nodes_mutex);
    if (call_nodes.find(call_id) != call_nodes.end()) {
        return call_nodes.at(call_id);
    }
    return std::nullopt;
}

void Parser::resolve_call_types() {
    // The mutex will unlock by itself when it goes out of scope
    std::lock_guard<std::mutex> lock(call_nodes_mutex);
    for (const auto &[file_name, file] : Resolver::file_map) {
        // First, get the list of all function types inside this file
        std::unordered_map<std::string, std::string> function_types;
        for (const auto &node : file.definitions) {
            if (const auto *function_node = dynamic_cast<const FunctionNode *>(node.get())) {
                std::string return_type;
                for (const auto &ret_type : function_node->return_types) {
                    return_type += ret_type;
                }
                function_types.emplace(function_node->name, return_type);
            }
        }
        for (auto it = call_nodes.begin(); it != call_nodes.end();) {
            if (function_types.find(it->second->function_name) != function_types.end()) {
                it->second->type = function_types.at(it->second->function_name);
                ++it;
                call_nodes.erase(std::prev(it));
            } else {
                ++it;
            }
        }
    }
}
