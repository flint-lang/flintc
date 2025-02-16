#include "error/error.hpp"
#include "parser/parser.hpp"

#include "parser/signature.hpp"

std::optional<VariableNode> Parser::create_variable(Scope *scope, const token_list &tokens) {
    std::optional<VariableNode> var = std::nullopt;
    for (const auto &tok : tokens) {
        if (tok.type == TOK_IDENTIFIER) {
            std::string name = tok.lexme;
            if (scope->variable_types.find(name) == scope->variable_types.end()) {
                THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, tok.line, tok.column, name);
            }
            return VariableNode(name, scope->variable_types.at(name).first);
        }
    }
    return var;
}

std::optional<UnaryOpNode> Parser::create_unary_op(Scope *scope, const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

std::optional<LiteralNode> Parser::create_literal(const token_list &tokens) {
    for (const auto &tok : tokens) {
        if (Signature::tokens_match({tok}, Signature::literal)) {
            switch (tok.type) {
                default:
                    THROW_ERR(ErrValUnknownLiteral, ERR_PARSING, file_name, tok.line, tok.column, tok.lexme);
                    break;
                case TOK_INT_VALUE: {
                    std::variant<int, float, std::string, bool, char> value = std::stoi(tok.lexme);
                    return LiteralNode(value, "int");
                }
                case TOK_FLINT_VALUE: {
                    std::variant<int, float, std::string, bool, char> value = std::stof(tok.lexme);
                    return LiteralNode(value, "flint");
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

std::unique_ptr<CallNodeExpression> Parser::create_call_expression(Scope *scope, token_list &tokens) {
    auto call_node_args = create_call_base(scope, tokens);
    if (!call_node_args.has_value()) {
        THROW_ERR(ErrExprCallCreationFailed, ERR_PARSING, file_name, tokens);
    }
    std::unique_ptr<CallNodeExpression> call_node = std::make_unique<CallNodeExpression>( //
        std::get<0>(call_node_args.value()),                                              // name
        std::move(std::get<1>(call_node_args.value())),                                   // args
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
    std::optional<std::unique_ptr<ExpressionNode>> rhs = create_expression(scope, rhs_tokens);
    if (!lhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, lhs_tokens);
    }
    if (!rhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, rhs_tokens);
    }
    if (lhs.value()->type != rhs.value()->type) {
        THROW_ERR(ErrExprBinopTypeMismatch, ERR_PARSING, file_name, lhs_tokens, rhs_tokens, operator_token, lhs.value()->type,
            rhs.value()->type);
    }
    return BinaryOpNode(operator_token, lhs.value(), rhs.value(), lhs.value()->type);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_expression(Scope *scope, const token_list &tokens) {
    std::optional<std::unique_ptr<ExpressionNode>> expression = std::nullopt;
    token_list cond_tokens = clone_from_to(0, tokens.size(), tokens);
    // remove trailing semicolons
    for (auto iterator = cond_tokens.rbegin(); iterator != cond_tokens.rend(); ++iterator) {
        if (iterator->type == TOK_SEMICOLON) {
            cond_tokens.erase(std::next(iterator).base());
        } else {
            break;
        }
    }
    // remove surrounding parenthesis when the first and last token are '(' and ')'
    if (cond_tokens.begin()->type == TOK_LEFT_PAREN && cond_tokens.at(cond_tokens.size() - 1).type == TOK_RIGHT_PAREN) {
        cond_tokens.erase(cond_tokens.begin());
        cond_tokens.pop_back();
    }

    // TODO: A more advanced expression matching should be implemented, as this current implementation works not in all cases
    if (Signature::tokens_contain(cond_tokens, Signature::function_call)) {
        expression = create_call_expression(scope, cond_tokens);
    } else if (Signature::tokens_contain(cond_tokens, Signature::bin_op_expr)) {
        std::optional<BinaryOpNode> bin_op = create_binary_op(scope, cond_tokens);
        if (!bin_op.has_value()) {
            THROW_ERR(ErrExprBinopCreationFailed, ERR_PARSING, file_name, cond_tokens);
        }
        expression = std::make_unique<BinaryOpNode>(std::move(bin_op.value()));
    } else if (Signature::tokens_contain(cond_tokens, Signature::literal_expr)) {
        std::optional<LiteralNode> lit = create_literal(cond_tokens);
        if (!lit.has_value()) {
            THROW_ERR(ErrExprLitCreationFailed, ERR_PARSING, file_name, cond_tokens);
        }
        expression = std::make_unique<LiteralNode>(std::move(lit.value()));
    } else if (Signature::tokens_match(cond_tokens, Signature::unary_op_expr)) {
        std::optional<UnaryOpNode> unary_op = create_unary_op(scope, cond_tokens);
        if (!unary_op.has_value()) {
            THROW_ERR(ErrExprUnaryOpCreationFailed, ERR_PARSING, file_name, cond_tokens);
        }
        expression = std::make_unique<UnaryOpNode>(std::move(unary_op.value()));
    } else if (Signature::tokens_match(cond_tokens, Signature::variable_expr)) {
        std::optional<VariableNode> variable = create_variable(scope, cond_tokens);
        if (!variable.has_value()) {
            THROW_ERR(ErrExprVariableCreationFailed, ERR_PARSING, file_name, cond_tokens);
        }
        expression = std::make_unique<VariableNode>(std::move(variable.value()));
    } else {
        // Undefined expression
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, cond_tokens);
    }

    return expression;
}
