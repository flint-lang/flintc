#include "parser/parser.hpp"

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

FileNode Parser::parse_file(const std::filesystem::path &file) {
    std::string file_name = file.filename();
    PROFILE_SCOPE("Parsing file '" + file_name + "'");
    FileNode file_node(file_name);
    token_list tokens = Lexer(file).scan();
    // Consume all tokens and convert them to nodes
    while (!tokens.empty()) {
        Util::add_next_main_node(file_node, tokens);
    }
    return file_node;
}

std::optional<CallNodeBase *> Parser::get_call_from_id(unsigned int call_id) {
    if (call_nodes.find(call_id) != call_nodes.end()) {
        return call_nodes.at(call_id);
    }
    return std::nullopt;
}

void Parser::resolve_call_types() {
    for (const auto &[file_name, file] : Resolver::get_file_map()) {
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
