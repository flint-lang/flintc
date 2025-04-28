#include "lexer/builtins.hpp"
#include "lexer/lexer_utils.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "parser/signature.hpp"
#include "parser/type/data_type.hpp"
#include <algorithm>

bool Parser::add_next_main_node(FileNode &file_node, token_list &tokens) {
    token_list definition_tokens = get_definition_tokens(tokens);

    // Find the indentation of the definition
    int definition_indentation = 0;
    for (const TokenContext &tok : definition_tokens) {
        if (tok.type == TOK_INDENT) {
            definition_indentation++;
        } else {
            break;
        }
    }

    if (Matcher::tokens_contain(definition_tokens, Matcher::use_statement)) {
        if (definition_indentation > 0) {
            THROW_ERR(ErrUseStatementNotAtTopLevel, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        ImportNode import_node = create_import(definition_tokens);
        for (const auto &imported_file : imported_files) {
            if (imported_file->path == import_node.path) {
                // The same use statemnt was written twice in the same file
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
        }
        ImportNode *added_import = file_node.add_import(import_node);
        imported_files.emplace_back(added_import);
        return true;
    }

    token_list body_tokens = get_body_tokens(definition_indentation, tokens);
    if (Matcher::tokens_contain(definition_tokens, Matcher::function_definition)) {
        // Dont actually parse the function body, only its definition
        std::optional<FunctionNode> function_node = create_function(definition_tokens);
        if (!function_node.has_value()) {
            THROW_ERR(ErrDefFunctionCreation, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        FunctionNode *added_function = file_node.add_function(function_node.value());
        add_open_function({added_function, body_tokens});
        add_parsed_function(added_function, file_name);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::data_definition)) {
        std::optional<DataNode> data_node = create_data(definition_tokens, body_tokens);
        if (!data_node.has_value()) {
            THROW_ERR(ErrDefDataCreation, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        DataNode *added_data = file_node.add_data(data_node.value());
        add_parsed_data(added_data, file_name);
        if (!Type::add_type(std::make_shared<DataType>(added_data))) {
            THROW_ERR(ErrDefDataRedefinition, ERR_PARSING, file_name,                                    //
                definition_tokens.front().line, definition_tokens.front().column, data_node.value().name //
            );
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::func_definition)) {
        std::optional<FuncNode> func_node = create_func(definition_tokens, body_tokens);
        if (!func_node.has_value()) {
            THROW_ERR(ErrDefFuncCreation, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        file_node.add_func(func_node.value());
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::entity_definition)) {
        create_entity_type entity_creation = create_entity(definition_tokens, body_tokens);
        file_node.add_entity(entity_creation.first);
        if (entity_creation.second.has_value()) {
            std::unique_ptr<DataNode> data_node_ptr = std::move(entity_creation.second.value().first);
            std::unique_ptr<FuncNode> func_node_ptr = std::move(entity_creation.second.value().second);
            file_node.add_data(*data_node_ptr);
            file_node.add_func(*func_node_ptr);
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::enum_definition)) {
        EnumNode enum_node = create_enum(definition_tokens, body_tokens);
        file_node.add_enum(enum_node);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::error_definition)) {
        ErrorNode error_node = create_error(definition_tokens, body_tokens);
        file_node.add_error(error_node);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::variant_definition)) {
        VariantNode variant_node = create_variant(definition_tokens, body_tokens);
        file_node.add_variant(variant_node);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::test_definition)) {
        std::optional<TestNode> test_node = create_test(definition_tokens);
        if (!test_node.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        TestNode *added_test = file_node.add_test(test_node.value());
        add_open_test({added_test, body_tokens});
        add_parsed_test(added_test, file_name);
    } else {
        Debug::print_token_context_vector(definition_tokens, file_name);
        THROW_ERR(ErrUnexpectedDefinition, ERR_PARSING, file_name, definition_tokens);
        return false;
    }
    return true;
}

token_list Parser::get_definition_tokens(token_list &tokens) {
    // Scan through all the tokens and first extract all tokens from this line
    int end_index = 0;
    unsigned int start_line = tokens.at(0).line;
    for (const TokenContext &tok : tokens) {
        if (tok.line == start_line) {
            end_index++;
        } else {
            break;
        }
    }
    return extract_from_to(0, end_index, tokens);
}

token_list Parser::get_body_tokens(unsigned int definition_indentation, token_list &tokens) {
    int end_idx = 0;
    unsigned int current_line = tokens.at(0).line;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it->line != current_line || it == tokens.begin()) {
            current_line = it->line;
            std::optional<unsigned int> indents_maybe = Matcher::get_leading_indents(tokens, current_line);
            if (indents_maybe.has_value() && indents_maybe.value() <= definition_indentation) {
                break;
            }
        }
        end_idx++;
    }
    if (end_idx == 0) {
        THROW_ERR(ErrMissingBody, ERR_PARSING, file_name, tokens);
        std::exit(EXIT_FAILURE);
    }

    return extract_from_to(0, end_idx, tokens);
}

std::optional<std::tuple<                                          //
    std::string,                                                   //
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>>, //
    std::vector<std::shared_ptr<Type>>,                            //
    std::optional<bool>                                            //
    >>
Parser::create_call_or_initializer_base(Scope *scope, token_list &tokens) {
    std::optional<uint2> arg_range = Matcher::balanced_range_extraction(        //
        tokens, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
    );
    if (!arg_range.has_value()) {
        // Function call does not have opening and closing brackets ()
        return std::nullopt;
    }
    // remove the '(' and ')' tokens from the arg_range
    ++arg_range.value().first;
    --arg_range.value().second;

    std::string function_name;
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> arguments;

    for (const auto &tok : tokens) {
        // Get the function name
        if (tok.type == TOK_IDENTIFIER) {
            function_name = tok.lexme;
            break;
        }
    }

    // Arguments are separated by commas. When the arg_range.first == arg_range.second, no arguments are passed
    if (arg_range.value().first < arg_range.value().second) {
        // if the args contain at least one comma, it is known that multiple arguments are passed. If not, only one is
        // passed. But the comma must be present at the top-level and not within one of the balanced range groups
        const auto match_ranges = Matcher::get_match_ranges_in_range_outside_group(                                               //
            tokens, Matcher::token(TOK_COMMA), arg_range.value(), Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
        );
        if (!match_ranges.empty()) {
            for (auto match = match_ranges.begin();; ++match) {
                token_list argument_tokens;
                if (match == match_ranges.begin()) {
                    argument_tokens = clone_from_to(arg_range.value().first, match->first, tokens);
                } else if (match == match_ranges.end()) {
                    argument_tokens = clone_from_to((match - 1)->second, arg_range.value().second, tokens);
                } else {
                    argument_tokens = clone_from_to((match - 1)->second, match->first, tokens);
                }
                auto expression = create_expression(scope, argument_tokens);
                if (!expression.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, argument_tokens);
                    return std::nullopt;
                }
                arguments.emplace_back(std::move(expression.value()), false);
                if (match == match_ranges.end()) {
                    break;
                }
            }
        } else {
            token_list argument_tokens = extract_from_to(arg_range.value().first, arg_range.value().second, tokens);
            auto expression = create_expression(scope, argument_tokens);
            if (!expression.has_value()) {
                THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, argument_tokens);
                return std::nullopt;
            }
            arguments.emplace_back(std::move(expression.value()), false);
        }
    }

    // Get all the argument types
    std::vector<std::shared_ptr<Type>> argument_types;
    argument_types.reserve(arguments.size());
    for (auto &arg : arguments) {
        assert(std::holds_alternative<std::shared_ptr<Type>>(arg.first->type));
        argument_types.emplace_back(std::get<std::shared_ptr<Type>>(arg.first->type));
    }

    // Check if its a call to a builtin function, if it is, get the return type of said function
    const auto builtin_function = builtin_functions.find(function_name);
    if (builtin_function != builtin_functions.end()) {
        // Check if the function has the same arguments as the function expects
        const auto &function_overloads = builtin_function_types.at(builtin_function->second);
        // Check if any overloaded function exists
        std::optional<std::pair<std::vector<std::shared_ptr<Type>>, std::vector<std::shared_ptr<Type>>>> found_function = std::nullopt;

        for (const auto &[param_types_str, return_types_str] : function_overloads) {
            if (arguments.size() != param_types_str.size()) {
                continue;
            }
            std::vector<std::shared_ptr<Type>> param_types(param_types_str.size());
            std::transform(param_types_str.begin(), param_types_str.end(), param_types.begin(), Type::str_to_type);
            std::vector<std::shared_ptr<Type>> return_types(return_types_str.size());
            std::transform(return_types_str.begin(), return_types_str.end(), return_types.begin(), Type::str_to_type);
            if (argument_types != param_types) {
                continue;
            }
            auto param_it = param_types.begin();
            auto arg_it = argument_types.begin();
            bool is_same = true;
            while (arg_it != argument_types.end()) {
                if (*param_it != *arg_it) {
                    is_same = false;
                }
                ++param_it;
                ++arg_it;
            }
            if (is_same) {
                found_function = {param_types, return_types};
            }
        }
        if (!found_function.has_value()) {
            THROW_ERR(ErrExprCallWrongArgsBuiltin, ERR_PARSING, file_name,                        //
                clone_from_to(arg_range.value().first - 2, arg_range.value().second + 1, tokens), //
                function_name, argument_types);
            return std::nullopt;
        }
        std::vector<std::shared_ptr<Type>> types;
        types.reserve(found_function.value().second.size());
        for (const std::shared_ptr<Type> &type : found_function.value().second) {
            types.emplace_back(type);
        }
        return std::make_tuple(function_name, std::move(arguments), types, std::nullopt);
    }

    // Check if there exists a type with the name of the "function" call
    std::optional<std::shared_ptr<Type>> complex_type = Type::get_type_from_str(function_name);
    if (complex_type.has_value()) {
        // Its a data, entity or enum type
        if (const DataType *data_type = dynamic_cast<const DataType *>(complex_type.value().get())) {
            DataNode *const data_node = data_type->data_node;
            const auto initializer_fields = data_node->get_initializer_fields();
            // Now check if the initializer arguments are equal to the expected initializer fields
            if (initializer_fields.size() != arguments.size()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            for (size_t i = 0; i < arguments.size(); i++) {
                ExprType arg_type = arguments.at(i).first->type;
                if (std::holds_alternative<std::vector<std::shared_ptr<Type>>>(arg_type)) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                if (std::get<1>(initializer_fields.at(i)) != std::get<std::shared_ptr<Type>>(arg_type)) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            }

            std::vector<std::shared_ptr<Type>> return_type = {complex_type.value()};
            return std::make_tuple(   //
                data_node->name,      //
                std::move(arguments), //
                return_type,          //
                true                  //
            );
        }
    }

    // Its definitely not a data type initializer, so it must be a function call
    // Get the acutal function this call targets, and check if it even exists
    auto function = get_function_from_call(function_name, argument_types);
    if (!function.has_value()) {
        THROW_ERR(ErrExprCallOfUndefinedFunction, ERR_PARSING, file_name, tokens, function_name);
        return std::nullopt;
    }
    // Check if the argument count does match the parameter count
    const unsigned int param_count = function.value().first->parameters.size();
    const unsigned int arg_count = arguments.size();
    if (param_count != arg_count) {
        THROW_ERR(ErrExprCallWrongArgCount, ERR_PARSING, file_name, tokens, function_name, param_count, arg_count);
        return std::nullopt;
    }
    // If we came until here, the argument types definitely match the function parameter types, otherwise no function would have been found
    // Lastly, update the arguments of the call with the information of the function definition, if the arguments should be references
    // Every non-primitive type is always a reference (for now)
    for (auto &arg : arguments) {
        arg.second = keywords.find(std::get<std::shared_ptr<Type>>(arg.first->type)->to_string()) == keywords.end();
    }

    return std::make_tuple(function_name, std::move(arguments), function.value().first->return_types, std::nullopt);
}

std::optional<std::tuple<Token, std::unique_ptr<ExpressionNode>, bool>> Parser::create_unary_op_base(Scope *scope, token_list &tokens) {
    remove_leading_garbage(tokens);
    remove_trailing_garbage(tokens);
    // For an unary operator to work, the tokens now must have at least two tokens
    if (tokens.size() < 2) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Check if the unary operator is defined to the left of the expression or the right of it
    bool is_left;
    const uint2 left_range = {0, 1};
    const uint2 right_range = {tokens.size() - 1, tokens.size()};
    if (Matcher::tokens_contain_in_range(tokens, Matcher::unary_operator, left_range)) {
        is_left = true;
    } else if (Matcher::tokens_contain_in_range(tokens, Matcher::unary_operator, right_range)) {
        is_left = false;
    } else {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Extract the operator token
    token_list operator_tokens = extract_from_to(         //
        is_left ? left_range.first : right_range.first,   //
        is_left ? left_range.second : right_range.second, //
        tokens                                            //
    );
    assert(operator_tokens.size() == 1);
    Token operator_token = operator_tokens.at(0).type;

    // All other tokens now are the expression
    auto expression = create_expression(scope, tokens);
    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    return std::make_tuple(operator_token, std::move(expression.value()), is_left);
}

std::optional<std::tuple<std::shared_ptr<Type>, std::string, std::string, unsigned int, std::shared_ptr<Type>>>
Parser::create_field_access_base( //
    Scope *scope,                 //
    token_list &tokens            //
) {
    remove_leading_garbage(tokens);

    // The first token is the accessed variable name
    assert(tokens.front().type == TOK_IDENTIFIER);
    const std::string var_name = tokens.front().lexme;
    tokens.erase(tokens.begin());

    // The next token should be a dot, skip it
    assert(tokens.front().type == TOK_DOT);
    tokens.erase(tokens.begin());

    // Then there should be the name of the field to access
    assert(tokens.front().type == TOK_IDENTIFIER);
    const std::string field_name = tokens.front().lexme;
    tokens.erase(tokens.begin());

    // Now get the data type from the data variables name
    const std::optional<std::shared_ptr<Type>> data_type = scope->get_variable_type(var_name);
    if (!data_type.has_value()) {
        // The variable doesnt exist
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // If the variable is of type `str`, the only valid access is its `length` variable
    if (data_type.value()->to_string() == "str") {
        if (field_name != "length") {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_tuple(data_type.value(), var_name, "length", 0, Type::get_simple_type("u64"));
    } else if (dynamic_cast<const ArrayType *>(data_type.value().get())) {
        if (field_name != "length") {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_tuple(data_type.value(), var_name, "length", 1, Type::get_simple_type("u64"));
    }
    std::optional<DataNode *> data_node = get_data_definition(                        //
        file_name, data_type.value()->to_string(), imported_files, std::nullopt, true //
    );
    if (!data_node.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now we can check if the given field name exists in the data
    if (data_node.value()->fields.find(field_name) == data_node.value()->fields.end()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::shared_ptr<Type> field_type = data_node.value()->fields.at(field_name).first;
    unsigned int field_id = 0;
    for (; field_id < data_node.value()->order.size(); field_id++) {
        if (data_node.value()->order.at(field_id) == field_name) {
            break;
        }
    }
    if (field_id == data_node.value()->order.size()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return std::make_tuple(data_type.value(), var_name, field_name, field_id, field_type);
}

std::optional<
    std::tuple<std::shared_ptr<Type>, std::string, std::vector<std::string>, std::vector<unsigned int>, std::vector<std::shared_ptr<Type>>>>
Parser::create_grouped_access_base( //
    Scope *scope,                   //
    token_list &tokens              //
) {
    remove_leading_garbage(tokens);

    // The first token is the accessed variable name
    assert(tokens.front().type == TOK_IDENTIFIER);
    const std::string var_name = tokens.front().lexme;
    tokens.erase(tokens.begin());

    // The next token should be a dot, skip it
    assert(tokens.front().type == TOK_DOT);
    tokens.erase(tokens.begin());

    // Then there should be an opening parenthesis the name of the field to access
    assert(tokens.front().type == TOK_LEFT_PAREN);
    tokens.erase(tokens.begin());

    // Now, extract the names of all accessed fields
    std::vector<std::string> field_names;
    while (!tokens.empty() && tokens.front().type != TOK_RIGHT_PAREN) {
        if (tokens.front().type == TOK_IDENTIFIER) {
            field_names.emplace_back(tokens.front().lexme);
        }
        tokens.erase(tokens.begin());
    }
    if (field_names.empty()) {
        // Empty group access
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now remove the right paren
    assert(tokens.front().type == TOK_RIGHT_PAREN);
    tokens.erase(tokens.begin());

    // Now get the data type from the data variables name
    const std::optional<std::shared_ptr<Type>> data_type = scope->get_variable_type(var_name);
    if (!data_type.has_value()) {
        // The variable doesnt exist
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    std::optional<DataNode *> data_node = get_data_definition(                        //
        file_name, data_type.value()->to_string(), imported_files, std::nullopt, true //
    );
    if (!data_node.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Next, get the field types and field ids from the data node
    std::vector<std::shared_ptr<Type>> field_types;
    std::vector<unsigned int> field_ids;
    for (const auto &field_name : field_names) {
        // Now we can check if the given field name exists in the data
        if (data_node.value()->fields.find(field_name) == data_node.value()->fields.end()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        field_types.emplace_back(data_node.value()->fields.at(field_name).first);
        unsigned int field_id = 0;
        for (; field_id < data_node.value()->order.size(); field_id++) {
            if (data_node.value()->order.at(field_id) == field_name) {
                break;
            }
        }
        if (field_id == data_node.value()->order.size()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        field_ids.emplace_back(field_id);
    }

    return std::make_tuple(data_type.value(), var_name, field_names, field_ids, field_types);
}
