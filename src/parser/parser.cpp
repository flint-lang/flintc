#include "parser/parser.hpp"

#include "debug.hpp"
#include "lexer/lexer.hpp"
#include "profiler.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

std::vector<Parser> Parser::instances;
std::map<unsigned int, CallNodeBase *> Parser::parsed_calls;
std::mutex Parser::parsed_calls_mutex;
std::vector<std::pair<FunctionNode *, std::string>> Parser::parsed_functions;
std::mutex Parser::parsed_functions_mutex;

Parser *Parser::create(const std::filesystem::path &file) {
    instances.emplace_back(Parser(file));
    return &instances.back();
}

std::optional<FileNode> Parser::parse() {
    std::string file_name = file.filename();
    PROFILE_SCOPE("Parsing file '" + file_name + "'");
    FileNode file_node(file_name);
    token_list tokens = Lexer(file).scan();
    Debug::print_token_context_vector(tokens, file_name);
    // Consume all tokens and convert them to nodes
    bool had_failure = false;
    while (!tokens.empty()) {
        if (!add_next_main_node(file_node, tokens)) {
            had_failure = true;
        }
    }
    if (had_failure) {
        return std::nullopt;
    }
    return file_node;
}

std::optional<CallNodeBase *> Parser::get_call_from_id(unsigned int call_id) {
    // The mutex will unlock by itself when it goes out of scope
    std::lock_guard<std::mutex> lock(parsed_calls_mutex);
    if (parsed_calls.find(call_id) != parsed_calls.end()) {
        return parsed_calls.at(call_id);
    }
    return std::nullopt;
}

bool Parser::parse_all_open_functions() {
    for (Parser &parser : instances) {
        auto next = parser.get_next_open_function();
        while (next.has_value()) {
            auto &[function, tokens] = next.value();
            // Create the body and add the body statements to the created scope
            auto body_statements = parser.create_body(function->scope.get(), tokens);
            if (!body_statements.has_value()) {
                THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, parser.file_name, tokens);
                return false;
            }
            function->scope.get()->body = std::move(body_statements.value());
            std::cout << "NEXT: " << function->name << ": " << std::endl;

            next = parser.get_next_open_function();
        }
    }
    instances.clear();
    return true;
}
