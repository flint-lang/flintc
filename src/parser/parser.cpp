#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "lexer/lexer.hpp"
#include "parallel.hpp"
#include "profiler.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iterator>
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
std::unordered_map<std::string, std::vector<DataNode *>> Parser::parsed_data;
std::mutex Parser::parsed_data_mutex;

Parser *Parser::create(const std::filesystem::path &file) {
    instances.emplace_back(Parser(file));
    return &instances.back();
}

std::optional<FileNode> Parser::parse() {
    PROFILE_SCOPE("Parse file '" + file_name + "'");
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

std::optional<DataNode *> Parser::get_data_definition( //
    const std::string &file_name,                      //
    const std::string &data_name,                      //
    const std::vector<ImportNode *> &imports,          //
    const std::vector<std::string> &arg_types,         //
    const bool is_known_data_type                      //
) {
    std::lock_guard<std::mutex> lock(parsed_data_mutex);

    // First, get all file names of the imported files
    std::vector<std::string> imported_files;
    for (const auto &import : imports) {
        if (std::holds_alternative<std::string>(import->path)) {
            imported_files.emplace_back(std::get<std::string>(import->path));
        }
    }

    // Then, extract all the data from all visible files into one large list
    std::vector<DataNode *> visible_data;
    if (parsed_data.find(file_name) != parsed_data.end()) {
        // Add all data definitions of this file
        visible_data = parsed_data.at(file_name);
    }
    for (const auto &[imported_file_name, data_list] : parsed_data) {
        if (std::find(imported_files.begin(), imported_files.end(), imported_file_name) != imported_files.end()) {
            std::copy(data_list.begin(), data_list.end(), std::back_inserter(visible_data));
        }
    }

    // Then, remove all data nodes with a non-matching name from the list
    for (auto it = visible_data.begin(); it != visible_data.end();) {
        if ((*it)->name != data_name) {
            visible_data.erase(it);
        } else {
            ++it;
        }
    }

    // If the list is empty, there are no matching data definitions, maybe a missing use statement?
    if (visible_data.empty()) {
        if (is_known_data_type) {
            THROW_BASIC_ERR(ERR_PARSING);
        }
        return std::nullopt;
    }
    // If the list does contain more than one element, we have clushing data definitions, consider using the `as` keyword
    if (visible_data.size() > 1) {
        if (is_known_data_type) {
            THROW_BASIC_ERR(ERR_PARSING);
        }
        return std::nullopt;
    }

    // Now, get all initializer types of the data
    DataNode *data_definition = visible_data.front();
    std::vector<std::string> initializer_types;
    for (const auto &initializer_name : data_definition->order) {
        initializer_types.emplace_back(data_definition->fields.at(initializer_name).first);
    }

    // Now, check if the initializer types match the entered types
    if (initializer_types != arg_types) {
        if (is_known_data_type) {
            THROW_BASIC_ERR(ERR_PARSING);
        }
        return std::nullopt;
    }

    // Its this data definition
    return data_definition;
}
