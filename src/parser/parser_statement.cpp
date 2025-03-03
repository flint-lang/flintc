#include "parser/parser.hpp"

#include "error/error.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/signature.hpp"
#include "types.hpp"

#include <optional>

std::optional<std::unique_ptr<CallNodeStatement>> Parser::create_call_statement(Scope *scope, token_list &tokens) {
    auto call_node_args = create_call_base(scope, tokens);
    if (!call_node_args.has_value()) {
        THROW_ERR(ErrExprCallCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }
    std::unique_ptr<CallNodeStatement> call_node = std::make_unique<CallNodeStatement>( //
        std::get<0>(call_node_args.value()),                                            // name
        std::move(std::get<1>(call_node_args.value())),                                 // args
        std::get<2>(call_node_args.value())                                             // type
    );
    set_last_parsed_call(call_node->call_id, call_node.get());
    return call_node;
}

std::optional<ThrowNode> Parser::create_throw(Scope *scope, token_list &tokens) {
    unsigned int throw_id = 0;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it->type == TOK_THROW) {
            if ((it + 1) == tokens.end()) {
                THROW_ERR(ErrStmtThrowCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            throw_id = std::distance(tokens.begin(), it);
        }
    }
    token_list expression_tokens = extract_from_to(throw_id + 1, tokens.size(), tokens);
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, expression_tokens, "i32");
    if (!expr.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expression_tokens);
        return std::nullopt;
    }
    if (expr.value()->type != "i32") {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, expression_tokens, "i32", expr.value()->type);
        return std::nullopt;
    }
    return ThrowNode(expr.value());
}

std::optional<ReturnNode> Parser::create_return(Scope *scope, token_list &tokens) {
    unsigned int return_id = 0;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it->type == TOK_RETURN) {
            if ((it + 1) == tokens.end()) {
                THROW_ERR(ErrStmtReturnCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            return_id = std::distance(tokens.begin(), it);
        }
    }
    token_list expression_tokens = extract_from_to(return_id + 1, tokens.size(), tokens);
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, expression_tokens);
    if (!expr.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expression_tokens);
        return std::nullopt;
    }
    return ReturnNode(expr.value());
}

std::optional<std::unique_ptr<IfNode>> Parser::create_if(Scope *scope, std::vector<std::pair<token_list, token_list>> &if_chain) {
    assert(!if_chain.empty());
    std::pair<token_list, token_list> this_if_pair = if_chain.at(0);
    if_chain.erase(if_chain.begin());

    bool has_if = false;
    bool has_else = false;
    // Remove everything in front of the expression (\n, \t, else, if)
    for (auto it = this_if_pair.first.begin(); it != this_if_pair.first.end();) {
        if (it->type == TOK_ELSE) {
            has_else = true;
        } else if (it->type == TOK_IF) {
            has_if = true;
            this_if_pair.first.erase(it);
            break;
        }
        this_if_pair.first.erase(it);
    }
    // Remove everything after the expression (:, \n)
    for (auto rev_it = this_if_pair.first.rbegin(); rev_it != this_if_pair.first.rend();) {
        if (rev_it->type == TOK_COLON) {
            this_if_pair.first.erase(rev_it.base());
            break;
        }
        this_if_pair.first.erase(rev_it.base());
    }

    // Dangling else statement without if statement
    if (has_else && !has_if) {
        THROW_ERR(ErrStmtDanglingElse, ERR_PARSING, file_name, this_if_pair.first);
        return std::nullopt;
    }

    // Create the if statements condition and body statements
    std::optional<std::unique_ptr<ExpressionNode>> condition = create_expression(scope, this_if_pair.first, "bool");
    if (!condition.has_value()) {
        // Invalid expression inside if statement
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, this_if_pair.first);
        return std::nullopt;
    }
    if (condition.value()->type != "bool") {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, this_if_pair.first, "bool", condition.value()->type);
        return std::nullopt;
    }
    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>(scope);
    auto body_statements = create_body(body_scope.get(), this_if_pair.second);
    if (!body_statements.has_value()) {
        THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, file_name, this_if_pair.second);
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());
    std::optional<std::variant<std::unique_ptr<IfNode>, std::unique_ptr<Scope>>> else_scope = std::nullopt;

    // Check if the chain contains any values (more if blocks in the chain) and parse them accordingly
    if (!if_chain.empty()) {
        if (Signature::tokens_contain(if_chain.at(0).first, {TOK_IF})) {
            // 'else if'
            else_scope = create_if(scope, if_chain);
        } else {
            // 'else'
            std::unique_ptr<Scope> else_scope_ptr = std::make_unique<Scope>(scope);
            auto body_statements = create_body(else_scope_ptr.get(), if_chain.at(0).second);
            if (!body_statements.has_value()) {
                THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, file_name, if_chain.at(0).second);
                return std::nullopt;
            }
            else_scope_ptr->body = std::move(body_statements.value());
            else_scope = std::move(else_scope_ptr);
        }
    }

    return std::make_unique<IfNode>(condition.value(), body_scope, else_scope);
}

std::optional<std::unique_ptr<WhileNode>> Parser::create_while_loop( //
    Scope *scope,                                                    //
    const token_list &definition,                                    //
    token_list &body                                                 //
) {
    token_list condition_tokens = definition;
    // Remove everything in front of the expression (\n, \t, else, if)
    for (auto it = condition_tokens.begin(); it != condition_tokens.end();) {
        if (it->type == TOK_WHILE) {
            condition_tokens.erase(it);
            break;
        }
        condition_tokens.erase(it);
    }
    // Remove everything after the expression (:, \n)
    for (auto rev_it = condition_tokens.rbegin(); rev_it != condition_tokens.rend();) {
        if (rev_it->type == TOK_COLON) {
            condition_tokens.erase(rev_it.base());
            break;
        }
        condition_tokens.erase(rev_it.base());
    }

    std::optional<std::unique_ptr<ExpressionNode>> condition = create_expression(scope, condition_tokens, "bool");
    if (!condition.has_value()) {
        // Invalid expression inside while statement
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, condition_tokens);
        return std::nullopt;
    }

    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>(scope);
    auto body_statements = create_body(body_scope.get(), body);
    if (!body_statements.has_value()) {
        THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, file_name, body);
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());
    std::unique_ptr<WhileNode> while_node = std::make_unique<WhileNode>(condition.value(), body_scope);
    return while_node;
}

std::optional<std::unique_ptr<ForLoopNode>> Parser::create_for_loop( //
    Scope *scope,                                                    //
    const token_list &definition,                                    //
    token_list &body                                                 //
) {
    std::optional<std::unique_ptr<StatementNode>> initializer;
    std::optional<std::unique_ptr<ExpressionNode>> condition;
    std::optional<std::unique_ptr<StatementNode>> looparound;

    // Get the content of the for loop
    std::optional<uint2> expressions_range = Signature::get_next_match_range(definition, Signature::match_until_signature({{TOK_COLON}}));
    if (!expressions_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Get the actual expressions inside the for loops definition
    std::vector<uint2> expression_ranges = Signature::get_match_ranges_in_range( //
        definition,                                                              //
        Signature::match_until_signature({{TOK_SEMICOLON}}),                     //
        expressions_range.value()                                                //
    );
    if (expression_ranges.size() < 2) {
        // Too less expressions in the for loop definition
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    } else if (expression_ranges.size() > 2) {
        // Too many expressions in the for loop definition
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Parse the actual expressions / statements. Note that only non-scoped statements are valid within the for loops definition
    std::unique_ptr<Scope> definition_scope = std::make_unique<Scope>(scope);
    uint2 &initializer_range = expression_ranges.at(0);
    // "remove" everything including the 'for' keyword from the initializer statement
    // otherwise the for loop would create itself recursively
    for (unsigned int i = initializer_range.first; i < initializer_range.second; i++) {
        if (definition.at(i).type == TOK_FOR) {
            initializer_range.first = i + 1;
            break;
        }
    }
    // Parse the initializer statement
    token_list initializer_tokens = clone_from_to(initializer_range.first, initializer_range.second, definition);
    initializer = create_statement(definition_scope.get(), initializer_tokens);
    if (!initializer.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    definition_scope->body.emplace_back(std::move(initializer.value()));

    // Parse the loop condition expression
    uint2 &condition_range = expression_ranges.at(1);
    const token_list condition_tokens = clone_from_to(condition_range.first, condition_range.second, definition);
    condition = create_expression(definition_scope.get(), condition_tokens);
    if (!condition.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Parse the for loops body
    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>(definition_scope.get());
    auto body_statements = create_body(body_scope.get(), body);
    if (!body_statements.has_value()) {
        THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, file_name, body);
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());

    // Parse the looparound statement. The looparound statement is actually part of the body is the last statement of the body
    uint2 looparound_range{condition_range.second, expressions_range.value().second};
    token_list looparound_tokens = clone_from_to(looparound_range.first, looparound_range.second, definition);
    looparound = create_statement(body_scope.get(), looparound_tokens);
    if (!looparound.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    body_scope->body.emplace_back(std::move(looparound.value()));

    return std::make_unique<ForLoopNode>(condition.value(), definition_scope, body_scope);
}

std::optional<std::unique_ptr<ForLoopNode>> Parser::create_enh_for_loop(Scope *scope, const token_list &definition,
    const token_list &body) {
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

std::optional<std::unique_ptr<CatchNode>> Parser::create_catch( //
    Scope *scope,                                               //
    const token_list &definition,                               //
    token_list &body,                                           //
    std::vector<std::unique_ptr<StatementNode>> &statements     //
) {
    // First, extract everything left of the 'catch' statement and parse it as a normal (unscoped) statement
    std::optional<unsigned int> catch_id = std::nullopt;
    for (auto it = definition.begin(); it != definition.end(); ++it) {
        if (it->type == TOK_CATCH) {
            catch_id = std::distance(definition.begin(), it);
            break;
        }
    }
    if (!catch_id.has_value()) {
        THROW_ERR(ErrStmtDanglingCatch, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }

    token_list left_of_catch = clone_from_to(0, catch_id.value(), definition);
    std::optional<std::unique_ptr<StatementNode>> lhs = create_statement(scope, left_of_catch);
    if (!lhs.has_value()) {
        THROW_ERR(ErrStmtCreationFailed, ERR_PARSING, file_name, left_of_catch);
        return std::nullopt;
    }
    statements.emplace_back(std::move(lhs.value()));
    // Get the last parsed call and set the 'has_catch' property of the call node
    const unsigned int last_call_id = get_last_parsed_call_id();
    const std::optional<CallNodeBase *> last_call = get_call_from_id(last_call_id);
    if (!last_call.has_value()) {
        THROW_ERR(ErrStmtDanglingCatch, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }
    last_call.value()->has_catch = true;

    const token_list right_of_catch = clone_from_to(catch_id.value(), definition.size() - 1, definition);
    std::optional<std::string> err_var = std::nullopt;
    for (auto it = right_of_catch.begin(); it != right_of_catch.end(); ++it) {
        if (it->type == TOK_CATCH && (it + 1) != right_of_catch.end() && (it + 1)->type == TOK_IDENTIFIER) {
            err_var = (it + 1)->lexme;
        }
    }

    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>(scope);
    if (err_var.has_value()) {
        body_scope->add_variable_type(err_var.value(), "i32", body_scope->scope_id);
    }
    auto body_statements = create_body(body_scope.get(), body);
    if (!body_statements.has_value()) {
        THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, file_name, body);
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());

    return std::make_unique<CatchNode>(err_var, body_scope, last_call_id);
}

std::optional<std::unique_ptr<AssignmentNode>> Parser::create_assignment(Scope *scope, token_list &tokens) {
    auto iterator = tokens.begin();
    while (iterator != tokens.end()) {
        if (iterator->type == TOK_IDENTIFIER) {
            if ((iterator + 1)->type == TOK_EQUAL && (iterator + 2) != tokens.end()) {
                if (scope->variable_types.find(iterator->lexme) == scope->variable_types.end()) {
                    THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, iterator->line, iterator->column, iterator->lexme);
                    return std::nullopt;
                }
                std::string expected_type = scope->variable_types.at(iterator->lexme).first;
                // Parse the expression with the expected type passed into it
                token_list expression_tokens = extract_from_to(std::distance(tokens.begin(), iterator + 2), tokens.size(), tokens);
                std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, expression_tokens, expected_type);
                if (!expression.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expression_tokens);
                    return std::nullopt;
                }
                return std::make_unique<AssignmentNode>(expected_type, iterator->lexme, expression.value());
            } else {
                THROW_ERR(ErrStmtAssignmentCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
        }
        ++iterator;
    }
    return std::nullopt;
}

std::optional<DeclarationNode> Parser::create_declaration(Scope *scope, token_list &tokens, const bool &is_infered) {
    std::optional<DeclarationNode> declaration = std::nullopt;
    std::string type;
    std::string name;

    const Signature::signature lhs = Signature::match_until_signature({"(", TOK_EQUAL, "|", TOK_COLON_EQUAL, ")"});
    uint2 lhs_range = Signature::get_match_ranges(tokens, lhs).at(0);
    token_list lhs_tokens = extract_from_to(lhs_range.first, lhs_range.second, tokens);

    // Remove all \n and \t from the lhs tokens
    for (auto it = lhs_tokens.begin(); it != lhs_tokens.end();) {
        if (it->type == TOK_INDENT || it->type == TOK_EOL) {
            it = lhs_tokens.erase(it);
        } else {
            ++it;
        }
    }

    if (lhs_tokens.size() == 0) {
        THROW_ERR(ErrStmtDanglingEqualSign, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    if (is_infered) {
        auto expr = create_expression(scope, tokens);
        if (!expr.has_value()) {
            THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        for (auto it = lhs_tokens.begin(); it != lhs_tokens.end(); ++it) {
            if (it->type == TOK_IDENTIFIER && (it + 1)->type == TOK_COLON_EQUAL) {
                name = it->lexme;
                break;
            }
        }
        if (!scope->add_variable_type(name, expr.value()->type, scope->scope_id)) {
            // Variable shadowing
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, tokens.at(0).line, tokens.at(0).column, name);
            return std::nullopt;
        }
        declaration = DeclarationNode(expr.value()->type, name, expr.value());
    } else {
        unsigned int type_begin = 0;
        unsigned int type_end = lhs_tokens.size() - 2;
        for (auto it = lhs_tokens.begin(); it != lhs_tokens.end(); ++it) {
            if ((it + 1)->type == TOK_IDENTIFIER && (it + 2)->type == TOK_EQUAL) {
                const token_list type_tokens = extract_from_to(type_begin, type_end, lhs_tokens);
                type = Lexer::to_string(type_tokens);
                name = it->lexme;
                break;
            }
            ++it;
        }
        if (!scope->add_variable_type(name, type, scope->scope_id)) {
            // Variable shadowing
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, tokens.at(0).line, tokens.at(0).column, name);
            return std::nullopt;
        }
        auto expr = create_expression(scope, tokens, type);
        if (!expr.has_value()) {
            THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        declaration = DeclarationNode(type, name, expr.value());
    }

    return declaration;
}

std::optional<UnaryOpStatement> Parser::create_unary_op_statement(Scope *scope, token_list &tokens) {
    auto unary_op_values = create_unary_op_base(scope, tokens);
    if (!unary_op_values.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    return UnaryOpStatement(                  //
        std::get<0>(unary_op_values.value()), //
        std::get<1>(unary_op_values.value()), //
        std::get<2>(unary_op_values.value())  //
    );
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_statement(Scope *scope, token_list &tokens) {
    std::optional<std::unique_ptr<StatementNode>> statement_node = std::nullopt;

    if (Signature::tokens_contain(tokens, Signature::declaration_explicit)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, tokens, false);
        if (!decl.has_value()) {
            THROW_ERR(ErrStmtDeclarationCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Signature::tokens_contain(tokens, Signature::declaration_infered)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, tokens, true);
        if (!decl.has_value()) {
            THROW_ERR(ErrStmtDeclarationCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Signature::tokens_contain(tokens, Signature::assignment)) {
        std::optional<std::unique_ptr<AssignmentNode>> assign = create_assignment(scope, tokens);
        if (!assign.has_value()) {
            THROW_ERR(ErrStmtAssignmentCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::move(assign.value());
    } else if (Signature::tokens_contain(tokens, Signature::return_statement)) {
        std::optional<ReturnNode> return_node = create_return(scope, tokens);
        if (!return_node.has_value()) {
            THROW_ERR(ErrStmtReturnCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<ReturnNode>(std::move(return_node.value()));
    } else if (Signature::tokens_contain(tokens, Signature::throw_statement)) {
        std::optional<ThrowNode> throw_node = create_throw(scope, tokens);
        if (!throw_node.has_value()) {
            THROW_ERR(ErrStmtThrowCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<ThrowNode>(std::move(throw_node.value()));
    } else if (Signature::tokens_contain(tokens, Signature::function_call)) {
        statement_node = create_call_statement(scope, tokens);
    } else if (Signature::tokens_contain(tokens, Signature::unary_op_expr)) {
        std::optional<UnaryOpStatement> unary_op = create_unary_op_statement(scope, tokens);
        if (!unary_op.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<UnaryOpStatement>(std::move(unary_op.value()));
    } else {
        THROW_ERR(ErrStmtCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    return statement_node;
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_scoped_statement( //
    Scope *scope,                                                              //
    token_list &definition,                                                    //
    token_list &body,                                                          //
    std::vector<std::unique_ptr<StatementNode>> &statements                    //
) {
    std::optional<std::unique_ptr<StatementNode>> statement_node = std::nullopt;

    std::optional<unsigned int> indent_lvl_maybe = Signature::get_leading_indents(       //
        definition,                                                                      //
        definition.at(0).type == TOK_EOL ? definition.at(1).line : definition.at(0).line //
    );
    if (!indent_lvl_maybe.has_value()) {
        THROW_ERR(ErrMissingBody, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }
    token_list scoped_body = get_body_tokens(indent_lvl_maybe.value(), body);

    if (Signature::tokens_contain(definition, Signature::if_statement) ||
        Signature::tokens_contain(definition, Signature::else_if_statement) ||
        Signature::tokens_contain(definition, Signature::else_statement)) {
        std::vector<std::pair<token_list, token_list>> if_chain;
        if_chain.emplace_back(definition, scoped_body);
        if (Signature::tokens_contain(definition, {TOK_ELSE})) {
            // else or else if at top of if chain
            THROW_ERR(ErrStmtIfChainMissingIf, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }

        token_list next_definition = definition;
        while (true) {
            if (body.size() == 0) {
                break;
            }
            if (body.at(0).type == TOK_EOL) {
                body.erase(body.begin());
            }
            // Get the next indices of the next definition
            std::optional<uint2> next_line_range = Signature::get_tokens_line_range(body, body.at(0).line);
            if (!next_line_range.has_value()) {
                break;
            }
            // Check if the definition contains a 'else if' or 'else' statement. It cannot only contain a 'if' statement, as this then will
            // be part of its own IfNode and not part of this if-chain! This can be simplified to just check if the definition contains a
            // 'else' statement
            if (!Signature::tokens_contain_in_range(body, {TOK_ELSE}, next_line_range.value())) {
                break;
            }

            next_definition = extract_from_to(next_line_range.value().first, next_line_range.value().second, body);
            scoped_body = get_body_tokens(indent_lvl_maybe.value(), body);
            if_chain.emplace_back(next_definition, scoped_body);
        }

        std::optional<std::unique_ptr<IfNode>> if_node = create_if(scope, if_chain);
        if (!if_node.has_value()) {
            THROW_ERR(ErrStmtIfCreationFailed, ERR_PARSING, file_name, if_chain);
            return std::nullopt;
        }
        statement_node = std::move(if_node.value());
    } else if (Signature::tokens_contain(definition, Signature::for_loop)) {
        std::optional<std::unique_ptr<ForLoopNode>> for_loop = create_for_loop(scope, definition, scoped_body);
        if (!for_loop.has_value()) {
            THROW_ERR(ErrStmtForCreationFailed, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        statement_node = std::move(for_loop.value());
    } else if (Signature::tokens_contain(definition, Signature::par_for_loop) ||
        Signature::tokens_contain(definition, Signature::enhanced_for_loop)) {
        std::optional<std::unique_ptr<ForLoopNode>> enh_for_loop = create_enh_for_loop(scope, definition, scoped_body);
        if (!enh_for_loop.has_value()) {
            THROW_ERR(ErrStmtForCreationFailed, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        statement_node = std::move(enh_for_loop.value());
    } else if (Signature::tokens_contain(definition, Signature::while_loop)) {
        std::optional<std::unique_ptr<WhileNode>> while_loop = create_while_loop(scope, definition, scoped_body);
        if (!while_loop.has_value()) {
            THROW_ERR(ErrStmtWhileCreationFailed, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        statement_node = std::move(while_loop.value());
    } else if (Signature::tokens_contain(definition, Signature::catch_statement)) {
        std::optional<std::unique_ptr<CatchNode>> catch_node = create_catch(scope, definition, scoped_body, statements);
        if (!catch_node.has_value()) {
            THROW_ERR(ErrStmtCatchCreationFailed, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        statement_node = std::move(catch_node.value());
    } else if (Signature::tokens_contain(definition, Signature::function_call)) {
        statement_node = create_call_statement(scope, definition);
    } else {
        THROW_ERR(ErrStmtCreationFailed, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }

    return statement_node;
}

std::optional<std::vector<std::unique_ptr<StatementNode>>> Parser::create_body(Scope *scope, token_list &body) {
    std::vector<std::unique_ptr<StatementNode>> body_statements;
    const Signature::signature statement_signature = Signature::match_until_signature({"((", TOK_SEMICOLON, ")|(", TOK_COLON, "))"});

    std::optional<uint2> for_match;
    std::optional<token_list> for_definition_tokens;
    while (auto next_match = Signature::get_next_match_range(body, statement_signature)) {
        token_list statement_tokens = extract_from_to(next_match.value().first, next_match.value().second, body);
        if (Signature::tokens_contain(statement_tokens, {{TOK_FOR}})) {
            for_match = next_match;
            for_definition_tokens = std::move(statement_tokens);
            continue;
        } else if (for_match.has_value()) {
            for_match.value().second = next_match.value().second;
            std::copy(statement_tokens.begin(), statement_tokens.end(), std::back_inserter(for_definition_tokens.value()));
            if (!Signature::tokens_contain(statement_tokens, {{TOK_COLON}})) {
                continue;
            } else {
                next_match = for_match.value();
                statement_tokens = std::move(for_definition_tokens.value());
            }
        }
        std::optional<std::unique_ptr<StatementNode>> next_statement = std::nullopt;
        if (Signature::tokens_contain(statement_tokens, {TOK_COLON})) {
            // --- SCOPED STATEMENT (IF, LOOPS, CATCH-BLOCK, SWITCH) ---
            next_statement = create_scoped_statement(scope, statement_tokens, body, body_statements);
        } else {
            // --- NORMAL STATEMENT ---
            next_statement = create_statement(scope, statement_tokens);
        }

        if (next_statement.has_value()) {
            body_statements.emplace_back(std::move(next_statement.value()));
        } else {
            THROW_ERR(ErrStmtCreationFailed, ERR_PARSING, file_name, statement_tokens);
            return std::nullopt;
        }

        for_match = std::nullopt;
        for_definition_tokens = std::nullopt;
    }

    return body_statements;
}
