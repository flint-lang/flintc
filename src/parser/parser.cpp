#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "globals.hpp"
#include "lexer/lexer.hpp"
#include "persistent_thread_pool.hpp"
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

Parser *Parser::create(const std::filesystem::path &file) {
    instances.emplace_back(Parser(file));
    return &instances.back();
}

std::optional<FileNode> Parser::parse() {
    PROFILE_SCOPE("Parse file '" + file_name + "'");
    FileNode file_node(file_name);
    token_list tokens = Lexer(file).scan();
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

std::optional<DataNode *> Parser::get_data_definition(                  //
    const std::string &file_name,                                       //
    const std::string &data_name,                                       //
    const std::vector<ImportNode *> &imports,                           //
    const std::optional<std::vector<std::shared_ptr<Type>>> &arg_types, //
    const bool is_known_data_type                                       //
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

    DataNode *data_definition = visible_data.front();
    if (arg_types.has_value()) {
        // Check if the initializer types match the arg types
        std::vector<std::shared_ptr<Type>> initializer_types;
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
    }

    // Its this data definition
    return data_definition;
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
