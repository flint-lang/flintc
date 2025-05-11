#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "globals.hpp"
#include "lexer/lexer.hpp"
#include "parser/type/unknown_type.hpp"
#include "persistent_thread_pool.hpp"
#include "profiler.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

Parser *Parser::create(const std::filesystem::path &file) {
    instances.emplace_back(Parser(file));
    return &instances.back();
}

std::optional<FileNode> Parser::parse() {
    PROFILE_SCOPE("Parse file '" + file_name + "'");
    FileNode file_node(file_name);
    token_list tokens = Lexer(file).scan();
    if (tokens.empty()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    token_slice token_slice = {tokens.begin(), tokens.end()};
    if (PRINT_TOK_STREAM) {
        Debug::print_token_context_vector(token_slice, file_name);
    }
    // Consume all tokens and convert them to nodes
    bool had_failure = false;
    while (token_slice.first != token_slice.second) {
        if (!add_next_main_node(file_node, token_slice)) {
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

bool Parser::resolve_all_unknown_types() {
    // First go through all the parameters of the function and resolve their type if they are of unknown type
    for (auto &parser : instances) {
        // Resolve the parameter types of all functions
        for (FunctionNode *function : parser.get_open_functions()) {
            for (auto &param : function->parameters) {
                if (const UnknownType *unknown_type = dynamic_cast<const UnknownType *>(std::get<0>(param).get())) {
                    // Get the "real" type from the parameter type
                    auto type_maybe = Type::get_type_from_str(unknown_type->type_str);
                    if (!type_maybe.has_value()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    std::get<0>(param) = type_maybe.value();
                }
            }
            // The parameters are added to the list of variables to the functions scope, so we need to change the types there too
            for (auto &variable : function->scope->variables) {
                if (const UnknownType *unknown_type = dynamic_cast<const UnknownType *>(std::get<0>(variable.second).get())) {
                    auto type_maybe = Type::get_type_from_str(unknown_type->type_str);
                    if (!type_maybe.has_value()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    std::get<0>(variable.second) = type_maybe.value();
                }
            }
        }
    }
    std::lock_guard<std::mutex> lock(Parser::parsed_data_mutex);
    for (const auto &file : Parser::parsed_data) {
        for (DataNode *data : file.second) {
            for (auto &field : data->fields) {
                if (const UnknownType *unknown_type = dynamic_cast<const UnknownType *>(field.second.first.get())) {
                    auto type_maybe = Type::get_type_from_str(unknown_type->type_str);
                    if (!type_maybe.has_value()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    field.second.first = type_maybe.value();
                }
            }
        }
    }
    Type::clear_unknown_types();
    return true;
}

std::vector<FunctionNode *> Parser::get_open_functions() {
    std::vector<FunctionNode *> open_function_list;
    for (auto &open_function : open_functions_list) {
        open_function_list.emplace_back(std::get<0>(open_function));
    }
    return open_function_list;
}

bool Parser::parse_all_open_functions(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Functions");

    // Define a task to process a single function
    auto process_function = [](Parser &parser, FunctionNode *function, token_list tokens) -> bool {
        // Create the body and add the body statements to the created scope
        token_slice ts = {tokens.begin(), tokens.end()};
        auto body_statements = parser.create_body(function->scope.get(), ts);
        if (!body_statements.has_value()) {
            THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, parser.file_name, ts);
            return false;
        }
        function->scope.get()->body = std::move(body_statements.value());
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Enqueue tasks in the global thread pool
        std::vector<std::future<bool>> futures;
        // Iterate through all parsers and their open functions
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_function()) {
                auto &[function, tokens] = next.value();
                // Enqueue a task for each function
                futures.emplace_back(thread_pool.enqueue(process_function, std::ref(parser), function, std::move(tokens)));
            }
        }
        // Collect results from all tasks
        for (auto &future : futures) {
            result = result && future.get(); // Combine results using logical AND
        }
    } else {
        // Process functions sequentially
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_function()) {
                auto &[function, tokens] = next.value();
                result = result && process_function(parser, function, std::move(tokens));
            }
        }
    }
    return result;
}

bool Parser::parse_all_open_tests(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Tests");

    // Define a task to process a single test
    auto process_test = [](Parser &parser, TestNode *test, token_list tokens) -> bool {
        // Create the body and add the body statements to the created scope
        token_slice ts = {tokens.begin(), tokens.end()};
        auto body_statements = parser.create_body(test->scope.get(), ts);
        if (!body_statements.has_value()) {
            THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, parser.file_name, ts);
            return false;
        }
        test->scope.get()->body = std::move(body_statements.value());
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Enqueue tasks in the global thread pool
        std::vector<std::future<bool>> futures;
        // Iterate through all parsers and their open functions
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_test()) {
                auto &[test, tokens] = next.value();
                // Enqueue a task for each function
                futures.emplace_back(thread_pool.enqueue(process_test, std::ref(parser), test, std::move(tokens)));
            }
        }
        // Collect results from all tasks
        for (auto &future : futures) {
            result = result && future.get(); // Combine results using logical AND
        }
    } else {
        // Process functions sequentially
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_test()) {
                auto &[test, tokens] = next.value();
                result = result && process_test(parser, test, std::move(tokens));
            }
        }
    }
    return result;
}

std::string Parser::get_type_string(const token_list &tokens) {
    std::stringstream stream;
    for (const auto &tok : tokens) {
        stream << tok.lexme;
    }
    return stream.str();
}

std::optional<std::pair<FunctionNode *, std::string>> Parser::get_function_from_call( //
    const std::string &call_name,                                                     //
    const std::vector<std::shared_ptr<Type>> &arg_types                               //
) {
    std::lock_guard<std::mutex> lock(parsed_functions_mutex);
    std::vector<std::shared_ptr<Type>> fn_arg_types;
    for (const auto &[fn, file_name] : parsed_functions) {
        if (fn->name != call_name) {
            continue;
        }
        if (fn->parameters.size() != arg_types.size()) {
            continue;
        }
        fn_arg_types.reserve(fn->parameters.size());
        for (const auto &param : fn->parameters) {
            fn_arg_types.emplace_back(std::get<0>(param));
        }
        if (fn_arg_types != arg_types) {
            fn_arg_types.clear();
            continue;
        }
        return std::make_pair(fn, file_name);
    }
    return std::nullopt;
}

token_list Parser::extract_from_to(unsigned int from, unsigned int to, token_list &tokens) {
    token_list extraction = clone_from_to(from, to, tokens);
    tokens.erase(tokens.begin() + from, tokens.begin() + to);
    return extraction;
}

token_list Parser::clone_from_to(unsigned int from, unsigned int to, const token_list &tokens) {
    assert(to >= from);
    assert(to <= tokens.size());
    token_list extraction;
    if (to == from) {
        return extraction;
    }
    extraction.reserve(to - from);
    std::copy(tokens.begin() + from, tokens.begin() + to, std::back_inserter(extraction));
    return extraction;
}

token_list Parser::clone_from_slice(const token_slice &slice) {
    assert(slice.second - slice.first > 0);
    assert(slice.first != slice.second);
    token_list extraction;
    extraction.reserve(std::distance(slice.first, slice.second));
    std::copy(slice.first, slice.second, std::back_inserter(extraction));
    return extraction;
}
