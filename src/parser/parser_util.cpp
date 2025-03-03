#include "lexer/builtins.hpp"
#include "lexer/token.hpp"
#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "parser/signature.hpp"

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

    if (Signature::tokens_contain(definition_tokens, Signature::use_statement)) {
        if (definition_indentation > 0) {
            THROW_ERR(ErrUseStatementNotAtTopLevel, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        ImportNode import_node = create_import(definition_tokens);
        file_node.add_import(import_node);
        return true;
    }

    token_list body_tokens = get_body_tokens(definition_indentation, tokens);
    if (Signature::tokens_contain(definition_tokens, Signature::function_definition)) {
        // Dont actually parse the function body, only its definition
        std::optional<FunctionNode> function_node = create_function(definition_tokens, body_tokens);
        if (!function_node.has_value()) {
            THROW_ERR(ErrDefFunctionCreation, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        FunctionNode *added_function = file_node.add_function(function_node.value());
        add_open_function({added_function, body_tokens});
        add_parsed_function(added_function, file_name);
    } else if (Signature::tokens_contain(definition_tokens, Signature::data_definition)) {
        DataNode data_node = create_data(definition_tokens, body_tokens);
        file_node.add_data(data_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::func_definition)) {
        std::optional<FuncNode> func_node = create_func(definition_tokens, body_tokens);
        if (!func_node.has_value()) {
            THROW_ERR(ErrDefFuncCreation, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        file_node.add_func(func_node.value());
    } else if (Signature::tokens_contain(definition_tokens, Signature::entity_definition)) {
        create_entity_type entity_creation = create_entity(definition_tokens, body_tokens);
        file_node.add_entity(entity_creation.first);
        if (entity_creation.second.has_value()) {
            std::unique_ptr<DataNode> data_node_ptr = std::move(entity_creation.second.value().first);
            std::unique_ptr<FuncNode> func_node_ptr = std::move(entity_creation.second.value().second);
            file_node.add_data(*data_node_ptr);
            file_node.add_func(*func_node_ptr);
        }
    } else if (Signature::tokens_contain(definition_tokens, Signature::enum_definition)) {
        EnumNode enum_node = create_enum(definition_tokens, body_tokens);
        file_node.add_enum(enum_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::error_definition)) {
        ErrorNode error_node = create_error(definition_tokens, body_tokens);
        file_node.add_error(error_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::variant_definition)) {
        VariantNode variant_node = create_variant(definition_tokens, body_tokens);
        file_node.add_variant(variant_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::test_definition)) {
        std::optional<TestNode> test_node = create_test(definition_tokens, body_tokens);
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
    int start_line = tokens.at(0).line;
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
    int current_line = tokens.at(0).line;
    int end_idx = 0;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it->line != current_line || it == tokens.begin()) {
            current_line = it->line;
            std::optional<unsigned int> indents_maybe = Signature::get_leading_indents(tokens, current_line);
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

std::optional<std::tuple<std::string, std::vector<std::unique_ptr<ExpressionNode>>, std::string>> Parser::create_call_base( //
    Scope *scope,                                                                                                           //
    token_list &tokens                                                                                                      //
) {
    std::optional<uint2> arg_range = Signature::balanced_range_extraction(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
    if (!arg_range.has_value()) {
        // Function call does not have opening and closing brackets ()
        return std::nullopt;
    }
    // remove the '(' and ')' tokens from the arg_range
    ++arg_range.value().first;
    --arg_range.value().second;

    std::string function_name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    std::vector<std::pair<unsigned int, unsigned int>> arg_ids;

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
        // passed
        if (Signature::tokens_contain_in_range(tokens, {{TOK_COMMA}}, arg_range.value())) {
            const auto match_ranges = Signature::get_match_ranges_in_range(tokens, {{TOK_COMMA}}, arg_range.value());
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
                    arguments.emplace_back(std::move(expression.value()));
                    arg_ids.emplace_back(*match);
                    if (match == match_ranges.end()) {
                        break;
                    }
                }
            }
        } else {
            token_list argument_tokens = extract_from_to(arg_range.value().first, arg_range.value().second, tokens);
            auto expression = create_expression(scope, argument_tokens);
            if (!expression.has_value()) {
                THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, argument_tokens);
                return std::nullopt;
            }
            arguments.emplace_back(std::move(expression.value()));
        }
    }

    // Get all the argument types
    std::vector<std::string> argument_types;
    argument_types.reserve(arguments.size());
    for (auto &arg : arguments) {
        argument_types.emplace_back(arg->type);
    }

    // Check if its a call to a builtin function, if it is, get the return type of said function
    const auto builtin_function = builtin_functions.find(function_name);
    if (builtin_function != builtin_functions.end()) {
        // Check if the function has the same arguments as the function expects
        const auto &function_overloads = builtin_function_types.at(builtin_function->second);
        // Check if any overloaded function exists
        std::optional<std::pair<std::vector<std::string_view>, std::vector<std::string_view>>> found_function = std::nullopt;

        for (const auto &[param_types, return_types] : function_overloads) {
            const std::vector<std::string> parameter_types(param_types.begin(), param_types.end());
            if (arguments.size() != param_types.size() || argument_types != parameter_types) {
                continue;
            }
            auto param_it = parameter_types.begin();
            auto arg_id_it = arg_ids.begin();
            auto arg_it = argument_types.begin();
            bool is_same = true;
            while (arg_it != argument_types.end()) {
                if (*param_it != *arg_it) {
                    is_same = false;
                }
                ++param_it;
                ++arg_id_it;
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
        return std::make_tuple(function_name, std::move(arguments), "");
    }
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
    // Check if all parameter types actually match the argument types
    // If we came until here, the arg count definitely matches the parameter count
    auto param_it = function.value().first->parameters.begin();
    auto arg_id_it = arg_ids.begin();
    auto arg_it = argument_types.begin();
    while (arg_it != argument_types.end()) {
        if (param_it->first != *arg_it) {
            THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name,          //
                clone_from_to(arg_id_it->first, arg_id_it->second, tokens), // tokens
                param_it->first,                                            // expected type
                *arg_it                                                     // actual type
            );
        }
        ++param_it;
        ++arg_id_it;
        ++arg_it;
    }

    // TODO: Change this eventually to support returning multiple types. Currently all return types are packed into one return type
    std::string return_type;
    auto &ret_types = function.value().first->return_types;
    for (auto it = ret_types.begin(); it != ret_types.end(); ++it) {
        if (it != ret_types.begin()) {
            return_type += "," + *it;
        } else {
            return_type += *it;
        }
    }
    return std::make_tuple(function_name, std::move(arguments), return_type);
}

std::optional<std::tuple<Token, std::unique_ptr<ExpressionNode>, bool>> Parser::create_unary_op_base(Scope *scope, token_list &tokens) {
    // The unary operator can either be at the beginning of the operation or at the end, but for that all unnecessary leading and trailing
    // tokens need to be removed
    // Remove all unnecessary leading tokens
    for (auto it = tokens.begin(); it != tokens.end();) {
        if (it->type == TOK_INDENT || it->type == TOK_EOL) {
            tokens.erase(it);
        } else {
            break;
        }
    }
    // Remove all unnecessary trailing tokens
    for (auto it = tokens.rbegin(); it != tokens.rend();) {
        if (it->type == TOK_INDENT || it->type == TOK_EOL || it->type == TOK_SEMICOLON || it->type == TOK_COLON) {
            ++it;
            tokens.erase(std::prev(it).base());
        } else {
            break;
        }
    }
    // For an unary operator to work, the tokens now must have at least two tokens
    if (tokens.size() < 2) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Check if the unary operator is defined to the left of the expression or the right of it
    bool is_left;
    const uint2 left_range = {0, 1};
    const uint2 right_range = {tokens.size() - 1, tokens.size()};
    if (Signature::tokens_contain_in_range(tokens, Signature::unary_operator, left_range)) {
        is_left = true;
    } else if (Signature::tokens_contain_in_range(tokens, Signature::unary_operator, right_range)) {
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
