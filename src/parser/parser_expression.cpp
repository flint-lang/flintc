#include "error/error.hpp"
#include "lexer/builtins.hpp"
#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "parser/parser.hpp"

#include "parser/signature.hpp"
#include <algorithm>

std::optional<VariableNode> Parser::create_variable(Scope *scope, const token_list &tokens) {
    std::optional<VariableNode> var = std::nullopt;
    for (const auto &tok : tokens) {
        if (tok.type == TOK_IDENTIFIER) {
            std::string name = tok.lexme;
            if (scope->variable_types.find(name) == scope->variable_types.end()) {
                THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, tok.line, tok.column, name);
                return std::nullopt;
            }
            return VariableNode(name, scope->variable_types.at(name).first);
        }
    }
    return var;
}

std::optional<UnaryOpExpression> Parser::create_unary_op_expression(Scope *scope, token_list &tokens) {
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
        if (Signature::tokens_match({tok}, Signature::literal)) {
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

std::optional<std::unique_ptr<CallNodeExpression>> Parser::create_call_expression(Scope *scope, token_list &tokens) {
    auto call_node_args = create_call_base(scope, tokens);
    if (!call_node_args.has_value()) {
        THROW_ERR(ErrExprCallCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }
    std::unique_ptr<CallNodeExpression> call_node = std::make_unique<CallNodeExpression>( //
        std::get<0>(call_node_args.value()),                                              // name
        std::get<1>(call_node_args.value()),                                              // args
        std::get<2>(call_node_args.value())                                               // type
    );
    set_last_parsed_call(call_node->call_id, call_node.get());
    return call_node;
}

std::optional<BinaryOpNode> Parser::create_binary_op(Scope *scope, token_list &tokens) {
    unsigned int first_operator_idx = 0;
    unsigned int second_operator_idx = 0;
    Token first_operator_token = Token::TOK_EOL;
    Token second_operator_token = Token::TOK_EOL;
    for (auto iterator = tokens.begin(); iterator != tokens.end(); ++iterator) {
        // Check if there is a function call ahead
        if (Signature::tokens_contain_in_range(tokens, Signature::function_call,
                std::make_pair<unsigned int, unsigned int>(std::distance(tokens.begin(), iterator), tokens.size()))) {
            // skip the identifier(s)
            while (iterator->type != TOK_LEFT_PAREN) {
                ++iterator;
            }
            // skip the function call
            std::vector<uint2> next_groups = Signature::balanced_range_extraction_vec(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
            if (next_groups.empty()) {
                THROW_ERR(ErrUnclosedParen, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            for (const uint2 &group : next_groups) {
                if (group.first == std::distance(tokens.begin(), iterator)) {
                    iterator = tokens.begin() + group.second;
                }
            }
        }
        // Check if there is a group ahead, skip that one too
        if (iterator->type == TOK_LEFT_PAREN) {
            std::vector<uint2> next_groups = Signature::balanced_range_extraction_vec(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
            if (next_groups.empty()) {
                THROW_ERR(ErrUnclosedParen, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            for (const uint2 &group : next_groups) {
                if (group.first == std::distance(tokens.begin(), iterator)) {
                    iterator = tokens.begin() + group.second - 1;
                }
            }
        }
        // Check if the next token is a binary operator token
        if (Signature::tokens_match({{iterator->type}}, Signature::binary_operator)) {
            if (first_operator_token == TOK_EOL) {
                first_operator_idx = std::distance(tokens.begin(), iterator);
                first_operator_token = iterator->type;
            } else {
                second_operator_idx = std::distance(tokens.begin(), iterator);
                second_operator_token = iterator->type;
                break;
            }
        }
    }

    // Compare the token precenences, extract different areas depending on the precedences
    token_list lhs_tokens;
    Token operator_token = TOK_EOL;
    if (second_operator_token != TOK_EOL && token_precedence.at(first_operator_token) > token_precedence.at(second_operator_token)) {
        lhs_tokens = extract_from_to(0, second_operator_idx, tokens);
        operator_token = second_operator_token;
    } else {
        lhs_tokens = extract_from_to(0, first_operator_idx, tokens);
        operator_token = first_operator_token;
    }
    // 1 to skip the operator token
    token_list rhs_tokens = extract_from_to(1, tokens.size(), tokens);

    std::optional<std::unique_ptr<ExpressionNode>> lhs = create_expression(scope, lhs_tokens);
    if (!lhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, lhs_tokens);
        return std::nullopt;
    }
    std::optional<std::unique_ptr<ExpressionNode>> rhs = create_expression(scope, rhs_tokens, lhs.value()->type);
    if (!rhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, rhs_tokens);
        return std::nullopt;
    }
    // This check is done in the generator now, because of the implicit casting system
    // if (lhs.value()->type != rhs.value()->type) {
    //     THROW_ERR(ErrExprBinopTypeMismatch, ERR_PARSING, file_name, lhs_tokens, rhs_tokens, operator_token, lhs.value()->type,
    //         rhs.value()->type);
    //     return std::nullopt;
    // }
    // The binop expression is of type bool when its a relational operator
    if (Signature::tokens_contain({{operator_token}}, Signature::relational_binop)) {
        return BinaryOpNode(operator_token, lhs.value(), rhs.value(), "bool");
    }
    return BinaryOpNode(operator_token, lhs.value(), rhs.value(), lhs.value()->type);
}

std::optional<TypeCastNode> Parser::create_type_cast(Scope *scope, token_list &tokens) {
    std::optional<uint2> expr_range = Signature::balanced_range_extraction(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
    if (!expr_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Remove the parenthesis from the expression token ranges
    expr_range.value().first++;
    expr_range.value().second--;
    assert(expr_range.value().second > expr_range.value().first);

    // Get the type the expression needs to be converted to
    TokenContext type_token = TokenContext{TOK_EOF};
    for (auto iterator = tokens.begin(); iterator != tokens.end(); ++iterator) {
        if (Signature::tokens_match({*iterator}, Signature::type_prim) && std::next(iterator) != tokens.end() &&
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

    // Check if the type of the expression is castable at all
    if (primitive_casting_table.find(expression.value()->type) == primitive_casting_table.end()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::vector<std::string_view> &to_types = primitive_casting_table.at(expression.value()->type);
    if (std::find(to_types.begin(), to_types.end(), type_token.lexme) == to_types.end()) {
        // The given expression type cannot be cast to the wanted type
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return TypeCastNode(type_token.lexme, expression.value());
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_expression( //
    Scope *scope,                                                         //
    const token_list &tokens,                                             //
    const std::optional<std::string> expected_type                        //
) {
    std::optional<std::unique_ptr<ExpressionNode>> expression = std::nullopt;
    token_list expr_tokens = clone_from_to(0, tokens.size(), tokens);
    // remove trailing semicolons
    for (auto iterator = expr_tokens.rbegin(); iterator != expr_tokens.rend(); ++iterator) {
        if (iterator->type == TOK_SEMICOLON) {
            expr_tokens.erase(std::next(iterator).base());
        } else {
            break;
        }
    }
    // remove surrounding parenthesis when the first and last token are '(' and ')'
    if (expr_tokens.begin()->type == TOK_LEFT_PAREN && expr_tokens.at(expr_tokens.size() - 1).type == TOK_RIGHT_PAREN) {
        expr_tokens.erase(expr_tokens.begin());
        expr_tokens.pop_back();
    }

    // TODO: A more advanced expression matching should be implemented, as this current implementation works not in all cases
    if (Signature::tokens_contain(expr_tokens, Signature::function_call)) {
        expression = create_call_expression(scope, expr_tokens);
    } else if (Signature::tokens_contain(expr_tokens, Signature::unary_op_expr)) {
        std::optional<UnaryOpExpression> unary_op = create_unary_op_expression(scope, expr_tokens);
        if (!unary_op.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        expression = std::make_unique<UnaryOpExpression>(std::move(unary_op.value()));
    } else if (Signature::tokens_contain(expr_tokens, Signature::bin_op_expr)) {
        std::optional<BinaryOpNode> bin_op = create_binary_op(scope, expr_tokens);
        if (!bin_op.has_value()) {
            THROW_ERR(ErrExprBinopCreationFailed, ERR_PARSING, file_name, expr_tokens);
            return std::nullopt;
        }
        expression = std::make_unique<BinaryOpNode>(std::move(bin_op.value()));
    } else if (Signature::tokens_contain(expr_tokens, Signature::type_cast)) {
        std::optional<TypeCastNode> type_cast = create_type_cast(scope, expr_tokens);
        if (!type_cast.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        expression = std::make_unique<TypeCastNode>(std::move(type_cast.value()));
    } else if (Signature::tokens_contain(expr_tokens, Signature::literal_expr)) {
        std::optional<LiteralNode> lit = create_literal(expr_tokens);
        if (!lit.has_value()) {
            THROW_ERR(ErrExprLitCreationFailed, ERR_PARSING, file_name, expr_tokens);
            return std::nullopt;
        }
        expression = std::make_unique<LiteralNode>(std::move(lit.value()));
    } else if (Signature::tokens_match(expr_tokens, Signature::variable_expr)) {
        std::optional<VariableNode> variable = create_variable(scope, expr_tokens);
        if (!variable.has_value()) {
            THROW_ERR(ErrExprVariableCreationFailed, ERR_PARSING, file_name, expr_tokens);
        }
        expression = std::make_unique<VariableNode>(std::move(variable.value()));
    } else {
        // Undefined expression
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
        return std::nullopt;
    }

    // Check if the types are implicitely type castable, if they are, wrap the expression in a TypeCastNode
    if (expected_type.has_value() && expected_type.value() != expression.value()->type) {
        if (primitive_implicit_casting_table.find(expression.value()->type) != primitive_implicit_casting_table.end()) {
            const std::vector<std::string_view> &to_types = primitive_implicit_casting_table.at(expression.value()->type);
            if (std::find(to_types.begin(), to_types.end(), expected_type.value()) != to_types.end()) {
                expression = std::make_unique<TypeCastNode>(expected_type.value(), expression.value());
            }
        } else {
            THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
            return std::nullopt;
        }
    }

    return expression;
}
