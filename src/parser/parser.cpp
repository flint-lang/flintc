#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "lexer/lexer.hpp"
#include "parallel.hpp"
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
std::vector<std::pair<TestNode *, std::string>> Parser::parsed_tests;
std::mutex Parser::parsed_tests_mutex;

Parser *Parser::create(const std::filesystem::path &file) {
    instances.emplace_back(Parser(file));
    return &instances.back();
}

std::optional<FileNode> Parser::parse() {
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

bool Parser::parse_all_open_functions(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Functions");
    // Create a function object type that matches what reduce_on_all expects
    auto process_parser = [](Parser &parser) -> bool {
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
            next = parser.get_next_open_function();
        }
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Create explicit reducer and initializer functions
        auto reducer = [](bool a, bool b) -> bool { return a && b; };
        auto initializer = []() -> bool { return true; };
        // Call reduce_on_all with explicit template parameters if needed
        result = Parallel::reduce_on_all(process_parser, instances.begin(), instances.end(), reducer, initializer);
    } else {
        for (auto &parser : Parser::instances) {
            result = result && process_parser(parser);
        }
    }

    return result;
}

bool Parser::parse_all_open_tests(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Tests");
    // Create a function object type that matches what reduce_on_all expects
    auto process_parser = [](Parser &parser) -> bool {
        auto next = parser.get_next_open_test();
        while (next.has_value()) {
            auto &[test, tokens] = next.value();
            // Create the body and add the body statements to the created scope
            auto body_statements = parser.create_body(test->scope.get(), tokens);
            if (!body_statements.has_value()) {
                THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, parser.file_name, tokens);
                return false;
            }
            test->scope.get()->body = std::move(body_statements.value());
            next = parser.get_next_open_test();
        }
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Create explicit reducer and initializer functions
        auto reducer = [](bool a, bool b) -> bool { return a && b; };
        auto initializer = []() -> bool { return true; };
        // Call reduce_on_all with explicit template parameters if needed
        result = Parallel::reduce_on_all(process_parser, instances.begin(), instances.end(), reducer, initializer);
    } else {
        for (auto &parser : Parser::instances) {
            result = result && process_parser(parser);
        }
    }

    return result;
}

std::optional<std::pair<FunctionNode *, std::string>> Parser::get_function_from_call( //
    const std::string &call_name,                                                     //
    const std::vector<std::string> &arg_types                                         //
) {
    std::lock_guard<std::mutex> lock(parsed_functions_mutex);
    std::vector<std::string> fn_arg_types;
    for (const auto &[fn, file_name] : parsed_functions) {
        if (fn->name != call_name) {
            continue;
        }
        if (fn->parameters.size() != arg_types.size()) {
            continue;
        }
        fn_arg_types.reserve(fn->parameters.size());
        for (const auto &[type, name] : fn->parameters) {
            fn_arg_types.emplace_back(type);
        }
        if (fn_arg_types != arg_types) {
            fn_arg_types.clear();
            continue;
        }
        return std::make_pair(fn, file_name);
    }
    return std::nullopt;
}
