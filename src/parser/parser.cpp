#include "parser/parser.hpp"

#include "debug.hpp"
#include "globals.hpp"
#include "lexer/lexer.hpp"
#include "persistent_thread_pool.hpp"
#include "profiler.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>

Parser *Parser::create(const std::filesystem::path &file) {
    instances.emplace_back(Parser(file));
    return &instances.back();
}

Parser *Parser::create(const std::filesystem::path &file, const std::string &file_content) {
    PROFILE_CUMULATIVE("Parser::create");
    instances.emplace_back(Parser(file, file_content));
    return &instances.back();
}

bool Parser::file_exists_and_is_readable(const std::filesystem::path &file_path) {
    PROFILE_CUMULATIVE("Parser::file_exists_and_is_readable");
    std::ifstream file(file_path.string());
    return file.is_open() && !file.fail();
}

std::string Parser::load_file(const std::filesystem::path &file_path) {
    PROFILE_CUMULATIVE("Parser::load_file");
    std::ifstream file(file_path.string());
    if (!file) {
        throw std::runtime_error("Failed to load file " + file_path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::optional<FileNode *> Parser::parse() {
    PROFILE_SCOPE("Parse file '" + file_name + "'");
    file_node_ptr = std::make_unique<FileNode>(file_name);
    Lexer lexer(file_name, *source_code.get());
    file_node_ptr->tokens = lexer.scan();
    if (file_node_ptr->tokens.empty()) {
        return std::nullopt;
    }
    source_code_lines = lexer.lines;
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Lexer lines of file '" << file_name << "'" << DEFAULT << std::endl;
        unsigned int line_idx = 1;
        for (const auto &line : source_code_lines) {
            std::cout << std::to_string(line_idx) << " | " << std::string(line.second);
            line_idx++;
        }
    }
    token_slice token_slice = {file_node_ptr->tokens.begin(), file_node_ptr->tokens.end()};
    if (PRINT_TOK_STREAM) {
        Debug::print_token_context_vector(token_slice, file_name);
    }
    // Consume all tokens and convert them to nodes
    bool had_failure = false;
    while (token_slice.first != token_slice.second && token_slice.first->token != TOK_EOF) {
        if (!add_next_main_node(*(file_node_ptr.get()), token_slice)) {
            had_failure = true;
        }
    }
    if (had_failure) {
        return std::nullopt;
    }
    return file_node_ptr.get();
}

std::vector<std::pair<unsigned int, std::string_view>> Parser::get_source_code_lines() const {
    return source_code_lines;
}

bool Parser::resolve_all_unknown_types() {
    PROFILE_CUMULATIVE("Parser::resolve_all_unknown_types");
    // First go through all the parameters of the function and resolve their type if they are of unknown type
    for (auto &parser : instances) {
        // Resolve the parameter types of all functions
        for (FunctionNode *function : parser.get_open_functions()) {
            for (auto &param : function->parameters) {
                if (!Type::resolve_type(std::get<0>(param))) {
                    return false;
                }
            }
            // The parameters are added to the list of variables to the functions scope, so we need to change the types there too
            for (auto &variable : function->scope.value()->variables) {
                if (!Type::resolve_type(std::get<0>(variable.second))) {
                    return false;
                }
            }
        }
        for (auto &definition : parser.file_node_ptr->definitions) {
            if (definition->get_variation() == DefinitionNode::Variation::VARIANT) {
                auto *variant_node = definition->as<VariantNode>();
                for (auto &type : variant_node->possible_types) {
                    if (!Type::resolve_type(type.second)) {
                        return false;
                    }
                }
            }
        }
    }
    std::lock_guard<std::mutex> lock(Parser::parsed_data_mutex);
    for (const auto &file : Parser::parsed_data) {
        for (DataNode *data : file.second) {
            for (auto &field : data->fields) {
                if (!Type::resolve_type(std::get<1>(field))) {
                    return false;
                }
            }
        }
    }
    Type::clear_unknown_types();
    return true;
}

std::vector<FunctionNode *> Parser::get_open_functions() {
    PROFILE_CUMULATIVE("Parser::get_open_functions");
    std::vector<FunctionNode *> open_function_list;
    for (auto &open_function : open_functions_list) {
        open_function_list.emplace_back(std::get<0>(open_function));
    }
    return open_function_list;
}

bool Parser::parse_all_open_functions(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Functions");

    // Define a task to process a single function
    auto process_function = [](Parser &parser, FunctionNode *function, std::vector<Line> &body) -> bool {
        PROFILE_SCOPE("Process function '" + function->name + "'");
        // First, refine all the body lines
        collapse_types_in_lines(body, parser.file_node_ptr->tokens);
        if (DEBUG_MODE) {
            // Debug print the definition line as well as the body lines vector as one continuous print, like when printing the token lists
            std::cout << "\n"
                      << YELLOW << "[Debug Info] Printing refined body tokens of function: " << DEFAULT << function->name << std::endl;
            Debug::print_token_context_vector({body.front().tokens.first, body.back().tokens.second}, "DEFINITION");
        }
        // Create the body and add the body statements to the created scope
        auto body_statements = parser.create_body(function->scope.value(), body);
        if (!body_statements.has_value()) {
            return false;
        }
        function->scope.value()->body = std::move(body_statements.value());
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Enqueue tasks in the global thread pool
        std::vector<std::future<bool>> futures;
        // Iterate through all parsers and their open functions
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_function()) {
                auto &[function, body] = next.value();
                // Enqueue a task for each function
                futures.emplace_back(thread_pool.enqueue(process_function, std::ref(parser), function, std::ref(body)));
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
                auto &[function, body] = next.value();
                result = result && process_function(parser, function, std::ref(body));
            }
        }
    }
    return result;
}

bool Parser::parse_all_open_tests(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Tests");

    // Define a task to process a single test
    auto process_test = [](Parser &parser, TestNode *test, std::vector<Line> &body) -> bool {
        PROFILE_SCOPE("Process test '" + test->name + "'");
        // First, refine all the body lines
        collapse_types_in_lines(body, parser.file_node_ptr->tokens);
        if (DEBUG_MODE) {
            // Debug print the definition line as well as the body lines vector as one continuous print, like when printing the token lists
            std::cout << "\n" << YELLOW << "[Debug Info] Printing refined body tokens of test: " << DEFAULT << test->name << std::endl;
            Debug::print_token_context_vector({body.front().tokens.first, body.back().tokens.second}, "DEFINITION");
        }
        // Create the body and add the body statements to the created scope
        auto body_statements = parser.create_body(test->scope, body);
        if (!body_statements.has_value()) {
            return false;
        }
        test->scope->body = std::move(body_statements.value());
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Enqueue tasks in the global thread pool
        std::vector<std::future<bool>> futures;
        // Iterate through all parsers and their open functions
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_test()) {
                auto &[test, body] = next.value();
                // Enqueue a task for each function
                futures.emplace_back(thread_pool.enqueue(process_test, std::ref(parser), test, std::ref(body)));
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
                auto &[test, body] = next.value();
                result = result && process_test(parser, test, std::ref(body));
            }
        }
    }
    return result;
}

std::optional<std::tuple<std::string, overloads, std::optional<std::string>>> Parser::get_builtin_function( //
    const std::string &function_name,                                                                       //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules                         //
) {
    for (const auto &[module_name, import_node] : imported_core_modules) {
        const auto &module_functions = core_module_functions.at(module_name);
        if (module_functions.find(function_name) != module_functions.end()) {
            const auto &function_variants = module_functions.at(function_name);
            return std::make_tuple(module_name, function_variants, import_node->alias);
        }
    }
    return std::nullopt;
}

std::vector<std::pair<FunctionNode *, std::string>> Parser::get_function_from_call( //
    const std::string &call_name,                                                   //
    const std::vector<std::shared_ptr<Type>> &arg_types                             //
) {
    std::lock_guard<std::mutex> lock(parsed_functions_mutex);
    std::vector<std::shared_ptr<Type>> fn_arg_types;
    std::vector<std::pair<FunctionNode *, std::string>> found_functions;
    for (const auto &[fn, file_name] : parsed_functions) {
        if (fn->name != call_name) {
            continue;
        }
        if (fn->parameters.size() != arg_types.size()) {
            continue;
        }
        fn_arg_types.clear();
        fn_arg_types.reserve(fn->parameters.size());
        for (const auto &param : fn->parameters) {
            fn_arg_types.emplace_back(std::get<0>(param));
        }
        // Check if all parameters are of the type of the expected type, `int` and `float` comptime types can be used to call functions
        // expecting integer and floating point types directly (calling 'some_function(5)' with the function's arg type being u64 for
        // example)
        bool is_ok = true;
        for (size_t i = 0; i < arg_types.size(); i++) {
            if (arg_types[i] == fn_arg_types[i]) {
                continue;
            }
            if (arg_types[i] != fn_arg_types[i] && arg_types[i]->to_string() != "int" && arg_types[i]->to_string() != "float") {
                is_ok = false;
                break;
            }
            const CastDirection castability = check_primitive_castability(arg_types[i], fn_arg_types[i]);
            if (castability.kind != CastDirection::Kind::CAST_LHS_TO_RHS) {
                is_ok = false;
                break;
            }
        }
        if (!is_ok) {
            // Continue if the function's don't match
            continue;
        }
        found_functions.emplace_back(fn, file_name);
    }
    return found_functions;
}

token_list Parser::extract_from_to(unsigned int from, unsigned int to, token_list &tokens) {
    PROFILE_CUMULATIVE("Parser::extract_from_to");
    token_list extraction = clone_from_to(from, to, tokens);
    tokens.erase(tokens.begin() + from, tokens.begin() + to);
    return extraction;
}

token_list Parser::clone_from_to(unsigned int from, unsigned int to, const token_list &tokens) {
    PROFILE_CUMULATIVE("Parser::clone_from_to");
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
    PROFILE_CUMULATIVE("Parser::clone_from_slice");
    assert(slice.second - slice.first > 0);
    assert(slice.first != slice.second);
    token_list extraction;
    extraction.reserve(std::distance(slice.first, slice.second));
    std::copy(slice.first, slice.second, std::back_inserter(extraction));
    return extraction;
}

std::optional<const Parser *> Parser::get_instance_from_filename(const std::string &file) {
    PROFILE_CUMULATIVE("Parser::get_instance_from_filename");
    for (const auto &instance : instances) {
        if (instance.file_name == file) {
            return &instance;
        }
    }
    return std::nullopt;
}
