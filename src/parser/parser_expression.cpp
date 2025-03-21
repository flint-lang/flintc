#include "error/error.hpp"
#include "lexer/builtins.hpp"
#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "parser/parser.hpp"

#include "parser/signature.hpp"
#include <algorithm>
#include <memory>
#include <variant>

bool Parser::check_castability(std::unique_ptr<ExpressionNode> &lhs, std::unique_ptr<ExpressionNode> &rhs) {
    if (std::holds_alternative<std::string>(lhs->type) && std::holds_alternative<std::string>(rhs->type)) {
        // Both single type
        const std::string lhs_type = std::get<std::string>(lhs->type);
        const std::string rhs_type = std::get<std::string>(rhs->type);
        if (type_precedence.find(lhs_type) == type_precedence.end() || type_precedence.find(rhs_type) == type_precedence.end()) {
            // Not castable, wrong arg types
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const unsigned int lhs_precedence = type_precedence.at(lhs_type);
        const unsigned int rhs_precedence = type_precedence.at(rhs_type);
        if (lhs_precedence > rhs_precedence) {
            // The right type needs to be cast to the left type
            rhs = std::make_unique<TypeCastNode>(lhs_type, rhs);
        } else {
            // The left type needs to be cast to the right type
            lhs = std::make_unique<TypeCastNode>(rhs_type, lhs);
        }
    } else if (!std::holds_alternative<std::string>(lhs->type) && std::holds_alternative<std::string>(rhs->type)) {
        // Left group, right single type
        if (std::get<std::vector<std::string>>(lhs->type).size() > 1) {
            // Not castable, group and single type mismatch
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const std::string lhs_type = std::get<std::vector<std::string>>(lhs->type).at(0);
        const std::string rhs_type = std::get<std::string>(rhs->type);
        if (type_precedence.find(lhs_type) == type_precedence.end() || type_precedence.find(rhs_type) == type_precedence.end()) {
            // Not castable, wrong types
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const unsigned int lhs_precedence = type_precedence.at(lhs_type);
        const unsigned int rhs_precedence = type_precedence.at(rhs_type);
        if (lhs_precedence > rhs_precedence) {
            // The right type needs to be cast to the left type
            rhs = std::make_unique<TypeCastNode>(lhs_type, rhs);
            // Also change the type of the lhs from a single group type to a single value
            lhs->type = lhs_type;
        } else {
            // The left type needs to be cast to the right type
            lhs = std::make_unique<TypeCastNode>(rhs_type, lhs);
        }
    } else if (std::holds_alternative<std::string>(lhs->type) && !std::holds_alternative<std::string>(rhs->type)) {
        // Left single type, right group
        if (std::get<std::vector<std::string>>(rhs->type).size() > 1) {
            // Not castable, group and single type mismatch
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const std::string lhs_type = std::get<std::string>(lhs->type);
        const std::string rhs_type = std::get<std::vector<std::string>>(rhs->type).at(0);
        if (type_precedence.find(lhs_type) == type_precedence.end() || type_precedence.find(rhs_type) == type_precedence.end()) {
            // Not castable, wrong types
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const unsigned int lhs_precedence = type_precedence.at(lhs_type);
        const unsigned int rhs_precedence = type_precedence.at(rhs_type);
        if (lhs_precedence > rhs_precedence) {
            // The right type needs to be cast to the left type
            rhs = std::make_unique<TypeCastNode>(lhs_type, rhs);
        } else {
            // The left type needs to be cast to the right type
            lhs = std::make_unique<TypeCastNode>(rhs_type, lhs);
            // Also change the type of the rhs from a single group type to a single value
            rhs->type = rhs_type;
        }
    } else {
        // Both group
        // TODO
    }
    return true;
}

std::optional<VariableNode> Parser::create_variable(Scope *scope, const token_list &tokens) {
    std::optional<VariableNode> var = std::nullopt;
    for (const auto &tok : tokens) {
        if (tok.type == TOK_IDENTIFIER) {
            std::string name = tok.lexme;
            if (scope->variables.find(name) == scope->variables.end()) {
                THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, tok.line, tok.column, name);
                return std::nullopt;
            }
            return VariableNode(name, std::get<0>(scope->variables.at(name)));
        }
    }
    return var;
}

std::optional<UnaryOpExpression> Parser::create_unary_op_expression(Scope *scope, token_list &tokens) {
    remove_surrounding_paren(tokens);
    auto unary_op_values = create_unary_op_base(scope, tokens);
    if (!unary_op_values.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    return UnaryOpExpression(                 //
        std::get<0>(unary_op_values.value()), //
        std::get<1>(unary_op_values.value()), //
        std::get<2>(unary_op_values.value())  //
    );
}

std::optional<LiteralNode> Parser::create_literal(const token_list &tokens) {
    for (const auto &tok : tokens) {
        if (Signature::tokens_match({tok}, ESignature::LITERAL)) {
            switch (tok.type) {
                default:
                    THROW_ERR(ErrValUnknownLiteral, ERR_PARSING, file_name, tok.line, tok.column, tok.lexme);
                    return std::nullopt;
                    break;
                case TOK_INT_VALUE: {
                    std::variant<int, float, std::string, bool, char> value = std::stoi(tok.lexme);
                    return LiteralNode(value, "i32");
                }
                case TOK_FLINT_VALUE: {
                    std::variant<int, float, std::string, bool, char> value = std::stof(tok.lexme);
                    return LiteralNode(value, "f32");
                }
                case TOK_STR_VALUE: {
                    std::variant<int, float, std::string, bool, char> value = tok.lexme;
                    return LiteralNode(value, "str");
                }
                case TOK_TRUE: {
                    std::variant<int, float, std::string, bool, char> value = true;
                    return LiteralNode(value, "bool");
                }
                case TOK_FALSE: {
                    std::variant<int, float, std::string, bool, char> value = false;
                    return LiteralNode(value, "bool");
                }
                case TOK_CHAR_VALUE: {
                    std::variant<int, float, std::string, bool, char> value = tok.lexme[0];
                    return LiteralNode(value, "char");
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<std::variant<std::unique_ptr<CallNodeExpression>, std::unique_ptr<InitializerNode>>> //
Parser::create_call_or_initializer_expression(Scope *scope, token_list &tokens) {
    remove_surrounding_paren(tokens);
    auto call_or_init_node_args = create_call_or_initializer_base(scope, tokens);
    if (!call_or_init_node_args.has_value()) {
        THROW_ERR(ErrExprCallCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }
    // Now, check if its a initializer or a call
    if (std::get<3>(call_or_init_node_args.value()).has_value()) {
        // Its an initializer
        std::vector<std::unique_ptr<ExpressionNode>> args;
        for (auto &arg : std::get<1>(call_or_init_node_args.value())) {
            args.emplace_back(std::move(arg.first));
        }
        std::unique_ptr<InitializerNode> initializer_node = std::make_unique<InitializerNode>( //
            std::get<2>(call_or_init_node_args.value()).at(0),   // type vector (always of size 1 for initializers)
            std::get<3>(call_or_init_node_args.value()).value(), // is_data
            args                                                 // args
        );
        return initializer_node;
    } else {
        // Its a call
        std::unique_ptr<CallNodeExpression> call_node = std::make_unique<CallNodeExpression>( //
            std::get<0>(call_or_init_node_args.value()),                                      // name
            std::get<1>(call_or_init_node_args.value()),                                      // args
            std::get<2>(call_or_init_node_args.value())                                       // type
        );
        call_node->scope_id = scope->scope_id;
        set_last_parsed_call(call_node->call_id, call_node.get());
        return call_node;
    }
}

std::optional<TypeCastNode> Parser::create_type_cast(Scope *scope, token_list &tokens) {
    remove_surrounding_paren(tokens);
    std::optional<uint2> expr_range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
    if (!expr_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Remove the parenthesis from the expression token ranges
    expr_range.value().first++;
    expr_range.value().second--;
    assert(expr_range.value().second > expr_range.value().first);

    // Get the type the expression needs to be converted to
    TokenContext type_token = TokenContext{TOK_EOF, "", 0, 0};
    for (auto iterator = tokens.begin(); iterator != tokens.end(); ++iterator) {
        if (Signature::tokens_match({*iterator}, ESignature::TYPE_PRIM) && std::next(iterator) != tokens.end() &&
            std::next(iterator)->type == TOK_LEFT_PAREN) {
            type_token = *iterator;
            break;
        }
    }
    if (type_token.type == TOK_EOF) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Create the expression
    token_list expr_tokens = clone_from_to(expr_range.value().first, expr_range.value().second, tokens);
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, expr_tokens);
    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
        return std::nullopt;
    }
    if (!std::holds_alternative<std::string>(expression.value()->type)) {
        if (std::get<std::vector<std::string>>(expression.value()->type).size() == 1) {
            expression.value()->type = std::get<std::vector<std::string>>(expression.value()->type).at(0);
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }

    // Check if the type of the expression is castable at all
    if (primitive_casting_table.find(std::get<std::string>(expression.value()->type)) == primitive_casting_table.end()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::vector<std::string_view> &to_types = primitive_casting_table.at(std::get<std::string>(expression.value()->type));
    if (std::find(to_types.begin(), to_types.end(), type_token.lexme) == to_types.end()) {
        // The given expression type cannot be cast to the wanted type
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return TypeCastNode(type_token.lexme, expression.value());
}

std::optional<GroupExpressionNode> Parser::create_group_expression(Scope *scope, token_list &tokens) {
    // First, remove all leading and trailing garbage from the expression tokens
    remove_leading_garbage(tokens);
    remove_trailing_garbage(tokens);
    // Now, the first and the last token must be open and closing parenthesis respectively
    assert(tokens.begin()->type == TOK_LEFT_PAREN);
    assert(std::prev(tokens.end())->type == TOK_RIGHT_PAREN);
    // Remove the open and closing parenthesis
    tokens.erase(tokens.begin());
    tokens.pop_back();
    std::vector<std::unique_ptr<ExpressionNode>> expressions;
    static const std::string until_comma = Signature::get_regex_string(Signature::match_until_signature({{TOK_COMMA}}));
    while (!tokens.empty()) {
        // Check if the tokens contain any opening / closing parenthesis. If it doesnt, the group parsing can be simplified a lot
        if (!Signature::tokens_contain(tokens, TOK_LEFT_PAREN)) {
            // Extract all tokens until the comma if it contains a comma. If it does not contain a comma, we are at the end of the group
            if (!Signature::tokens_contain(tokens, TOK_COMMA)) {
                auto expr = create_expression(scope, tokens);
                if (!expr.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
                    return std::nullopt;
                }
                expressions.emplace_back(std::move(expr.value()));
                break;
            } else {
                std::optional<uint2> expr_range = Signature::get_next_match_range(tokens, until_comma);
                if (!expr_range.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                token_list expr_tokens = extract_from_to(expr_range.value().first, expr_range.value().second, tokens);
                // If the last token is a comma, it is removed
                if (expr_tokens.back().type == TOK_COMMA) {
                    expr_tokens.pop_back();
                }
                auto expr = create_expression(scope, expr_tokens);
                if (!expr.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
                    return std::nullopt;
                }
                expressions.emplace_back(std::move(expr.value()));
            }
        } else if (Signature::tokens_contain(tokens, TOK_COMMA)) {
            std::optional<uint2> expr_range = Signature::get_next_match_range(tokens, until_comma);
            if (!expr_range.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            token_list expr_tokens = extract_from_to(expr_range.value().first, expr_range.value().second, tokens);
            auto expr = create_expression(scope, expr_tokens);
            if (!expr.has_value()) {
                THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
                return std::nullopt;
            }
            expressions.emplace_back(std::move(expr.value()));
        } else {
            // THIS IS NOT A GROUPED EXPRESSION
            return std::nullopt;
        }
    }
    return GroupExpressionNode(expressions);
}

std::optional<DataAccessNode> Parser::create_data_access(Scope *scope, token_list &tokens) {
    auto field_access_base = create_field_access_base(scope, tokens);
    if (!field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return DataAccessNode(                      //
        std::get<0>(field_access_base.value()), // data_type
        std::get<1>(field_access_base.value()), // var_name
        std::get<2>(field_access_base.value()), // field_name
        std::get<3>(field_access_base.value()), // field_id
        std::get<4>(field_access_base.value())  // field_type
    );
}

std::optional<GroupedDataAccessNode> Parser::create_grouped_data_access(Scope *scope, token_list &tokens) {
    auto grouped_field_access_base = create_grouped_access_base(scope, tokens);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return GroupedDataAccessNode(                       //
        std::get<0>(grouped_field_access_base.value()), // data_type
        std::get<1>(grouped_field_access_base.value()), // var_name
        std::get<2>(grouped_field_access_base.value()), // field_names
        std::get<3>(grouped_field_access_base.value()), // field_ids
        std::get<4>(grouped_field_access_base.value())  // field_types
    );
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_pivot_expression( //
    Scope *scope,                                                               //
    token_list &tokens                                                          //
) {
    if (!Signature::tokens_match(tokens, ESignature::GROUP_EXPRESSION)) {
        remove_surrounding_paren(tokens);
    }

    // Try to parse primary expressions first (literal, variables)
    if (tokens.size() == 1) {
        if (Signature::tokens_match(tokens, ESignature::LITERAL)) {
            std::optional<LiteralNode> lit = create_literal(tokens);
            if (!lit.has_value()) {
                THROW_ERR(ErrExprLitCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            return std::make_unique<LiteralNode>(std::move(lit.value()));
        } else if (Signature::tokens_match(tokens, ESignature::VARIABLE_EXPR)) {
            std::optional<VariableNode> variable = create_variable(scope, tokens);
            if (!variable.has_value()) {
                THROW_ERR(ErrExprVariableCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            return std::make_unique<VariableNode>(std::move(variable.value()));
        }
    }

    if (Signature::tokens_match(tokens, ESignature::FUNCTION_CALL)) {
        auto range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        if (range.has_value() && range.value().second == tokens.size()) {
            // Its only a call, when the paren group of the function is at the very end of the tokens, otherwise there is something
            // located on the right of the call still
            auto call_or_initializer_expression = create_call_or_initializer_expression(scope, tokens);
            if (!call_or_initializer_expression.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            if (std::holds_alternative<std::unique_ptr<CallNodeExpression>>(call_or_initializer_expression.value())) {
                return std::move(std::get<std::unique_ptr<CallNodeExpression>>(call_or_initializer_expression.value()));
            } else {
                return std::move(std::get<std::unique_ptr<InitializerNode>>(call_or_initializer_expression.value()));
            }
        }
    } else if (Signature::tokens_match(tokens, ESignature::GROUP_EXPRESSION)) {
        auto range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        if (range.has_value() && range.value().first == 0 && range.value().second == tokens.size()) {
            std::optional<GroupExpressionNode> group = create_group_expression(scope, tokens);
            if (!group.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<GroupExpressionNode>(std::move(group.value()));
        }
    } else if (Signature::tokens_match(tokens, ESignature::TYPE_CAST)) {
        std::optional<TypeCastNode> type_cast = create_type_cast(scope, tokens);
        if (!type_cast.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<TypeCastNode>(std::move(type_cast.value()));
    } else if (Signature::tokens_match(tokens, ESignature::UNARY_OP_EXPR)) {
        // For it to be considered an unary operation, either right after the operator needs to come a paren group, or no other binop tokens
        auto range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        if (!Signature::tokens_contain(tokens, ESignature::BINARY_OPERATOR) ||
            (range.has_value() && range.value().second == tokens.size())) {
            std::optional<UnaryOpExpression> unary_op = create_unary_op_expression(scope, tokens);
            if (!unary_op.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<UnaryOpExpression>(std::move(unary_op.value()));
        }
    } else if (Signature::tokens_match(tokens, ESignature::DATA_ACCESS)) {
        if (tokens.size() == 3) {
            std::optional<DataAccessNode> data_access = create_data_access(scope, tokens);
            if (!data_access.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<DataAccessNode>(std::move(data_access.value()));
        }
    } else if (Signature::tokens_match(tokens, ESignature::GROUPED_DATA_ACCESS)) {
        auto range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        if (range.has_value() && range.value().first == 2 && range.value().second == tokens.size()) {
            std::optional<GroupedDataAccessNode> group_access = create_grouped_data_access(scope, tokens);
            if (!group_access.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<GroupedDataAccessNode>(std::move(group_access.value()));
        }
    }

    // Find the highest precedence operator
    unsigned int smallest_precedence = 100; //
    size_t pivot_pos = 0;
    Token pivot_token = TOK_EOL;

    // Find all possible binary operators at the root level
    // Start at the first index because the first token is never a unary operator
    for (size_t i = 1; i < tokens.size(); i++) {
        // Skip tokens inside parentheses or function calls
        if (tokens[i].type == TOK_LEFT_PAREN) {
            int paren_depth = 1;
            while (++i < tokens.size() && paren_depth > 0) {
                if (tokens[i].type == TOK_LEFT_PAREN) {
                    paren_depth++;
                } else if (tokens[i].type == TOK_RIGHT_PAREN) {
                    paren_depth--;
                }
            }
            if (i >= tokens.size()) {
                break;
            }
        }

        // Check if this is a operator and if no operator is to the left of this operator. If there is any operator to the left of this one,
        // this means that this operator is an unary operator
        if (token_precedence.find(tokens[i].type) != token_precedence.end() &&
            token_precedence.find(tokens[i - 1].type) == token_precedence.end()) {
            // Update smallest precedence if needed
            unsigned int precedence = token_precedence.at(tokens[i].type);
            if (precedence < smallest_precedence) {
                smallest_precedence = precedence;
                pivot_pos = i;
                pivot_token = tokens[i].type;
            }
        }
    }

    // If no binary operators, this is an error
    if (smallest_precedence == 0) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    // Extract the left and right parts of the expression
    token_list lhs_tokens = clone_from_to(0, pivot_pos, tokens);
    token_list rhs_tokens = clone_from_to(pivot_pos + 1, tokens.size(), tokens);

    // Recursively parse both sides
    auto lhs = create_pivot_expression(scope, lhs_tokens);
    if (!lhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, lhs_tokens);
        return std::nullopt;
    }

    auto rhs = create_pivot_expression(scope, rhs_tokens);
    if (!rhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, rhs_tokens);
        return std::nullopt;
    }

    // Check if all parameter types actually match the argument types
    // If we came until here, the arg count definitely matches the parameter count
    if (lhs.value()->type != rhs.value()->type) {
        if (!check_castability(lhs.value(), rhs.value())) {
            return std::nullopt;
        }
    }

    // Create the binary operator node
    if (Signature::tokens_contain({{pivot_token, "", 0, 0}}, ESignature::RELATIONAL_BINOP)) {
        return std::make_unique<BinaryOpNode>(pivot_token, lhs.value(), rhs.value(), "bool");
    }
    return std::make_unique<BinaryOpNode>(pivot_token, lhs.value(), rhs.value(), lhs.value()->type);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_expression(                   //
    Scope *scope,                                                                           //
    const token_list &tokens,                                                               //
    const std::optional<std::variant<std::string, std::vector<std::string>>> &expected_type //
) {
    token_list expr_tokens = clone_from_to(0, tokens.size(), tokens);
    remove_trailing_garbage(expr_tokens);

    // Parse expression using precedence levels
    auto expression = create_pivot_expression(scope, expr_tokens);

    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
        return std::nullopt;
    }

    // Check if the expressions result is a vector of size 1, if it is make its type into a direct string
    if (std::holds_alternative<std::vector<std::string>>(expression.value()->type)) {
        std::vector<std::string> &type = std::get<std::vector<std::string>>(expression.value()->type);
        if (type.size() == 1) {
            expression.value()->type = type.at(0);
        }
    }

    // Check if the types are implicitely type castable, if they are, wrap the expression in a TypeCastNode
    if (expected_type.has_value() && expected_type.value() != expression.value()->type) {
        if (std::holds_alternative<std::string>(expression.value()->type) && std::holds_alternative<std::string>(expected_type.value())) {
            const std::string type = std::get<std::string>(expression.value()->type);
            if (primitive_implicit_casting_table.find(type) != primitive_implicit_casting_table.end()) {
                const std::vector<std::string_view> &to_types = primitive_implicit_casting_table.at(type);
                if (std::find(to_types.begin(), to_types.end(), std::get<std::string>(expected_type.value())) != to_types.end()) {
                    expression = std::make_unique<TypeCastNode>(std::get<std::string>(expected_type.value()), expression.value());
                }
            } else {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                return std::nullopt;
            }
        } else if (std::holds_alternative<std::vector<std::string>>(expression.value()->type) &&
            std::holds_alternative<std::vector<std::string>>(expected_type.value())) {
            // TODO
        } else {
            // Lastly, check if the expression type is a group of size 1, if it is it could become a single type instead
            if (std::holds_alternative<std::vector<std::string>>(expression.value()->type)) {
                auto &expression_type = std::get<std::vector<std::string>>(expression.value()->type);
                if (expression_type.size() == 1) {
                    std::string type = expression_type.at(0);
                    expression.value()->type = type;
                } else {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                    return std::nullopt;
                }
            } else {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                return std::nullopt;
            }
        }
    }

    return expression;
}
