#include "parser/parser.hpp"

#include "error/error.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/switch_expression.hpp"
#include "parser/ast/expressions/switch_match_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/statements/break_node.hpp"
#include "parser/ast/statements/continue_node.hpp"
#include "parser/ast/statements/stacked_assignment.hpp"
#include "parser/ast/statements/stacked_grouped_assignment.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"
#include "types.hpp"

#include <iterator>
#include <optional>
#include <string>
#include <variant>

std::optional<std::unique_ptr<CallNodeStatement>> Parser::create_call_statement( //
    std::shared_ptr<Scope> scope,                                                //
    const token_slice &tokens,                                                   //
    const std::optional<std::string> &alias_base                                 //
) {
    token_slice tokens_mut = tokens;
    auto call_node_args = create_call_or_initializer_base(scope, tokens_mut, alias_base);
    if (!call_node_args.has_value()) {
        return std::nullopt;
    }
    assert(!std::get<3>(call_node_args.value()));
    std::unique_ptr<CallNodeStatement> call_node = std::make_unique<CallNodeStatement>( //
        std::get<0>(call_node_args.value()),                                            // name
        std::move(std::get<1>(call_node_args.value())),                                 // args
        std::get<4>(call_node_args.value()),                                            // can_throw
        std::get<2>(call_node_args.value())                                             // type
    );
    call_node->scope_id = scope->scope_id;
    last_parsed_call = call_node.get();
    return call_node;
}

std::optional<ThrowNode> Parser::create_throw(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    unsigned int throw_id = 0;
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->token == TOK_THROW) {
            if (std::next(it) == tokens.second) {
                // Missing expression in throw statement
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            throw_id = std::distance(tokens.first, it);
        }
    }
    token_slice expression_tokens = {tokens.first + throw_id + 1, tokens.second};
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, expression_tokens);
    if (!expr.has_value()) {
        return std::nullopt;
    }
    if (!dynamic_cast<const ErrorSetType *>(expr.value()->type.get())) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, expression_tokens, Type::get_primitive_type("anyerror"), expr.value()->type);
        return std::nullopt;
    }
    if (const VariableNode *var_node = dynamic_cast<const VariableNode *>(expr.value().get())) {
        // Add the current scope to the scope list in the variable we throw to determine that the variable is returned, excluding it from
        // being freed to early
        std::get<5>(scope->variables.at(var_node->name)).emplace_back(scope->scope_id);
    }
    return ThrowNode(expr.value());
}

std::optional<ReturnNode> Parser::create_return(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    // Get the return type of the function
    std::shared_ptr<Type> return_type = scope->get_variable_type("__flint_return_type").value();
    unsigned int return_id = 0;
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->token == TOK_RETURN) {
            if (std::next(it) == tokens.second && return_type->to_string() != "void") {
                // Return statement without expression for a function that returns a non-void value
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return_id = std::distance(tokens.first, it);
        }
    }

    token_slice expression_tokens = {tokens.first + return_id + 1, tokens.second};
    std::optional<std::unique_ptr<ExpressionNode>> return_expr;
    if (std::next(expression_tokens.first) == expression_tokens.second) {
        // This can be asserted because of the check above
        assert(return_type->to_string() == "void");
        return ReturnNode(return_expr);
    }
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, expression_tokens);
    if (!expr.has_value()) {
        return std::nullopt;
    }
    if (const VariableNode *variable_node = dynamic_cast<const VariableNode *>(expr.value().get())) {
        std::vector<unsigned int> &return_scopes = std::get<5>(scope->variables.at(variable_node->name));
        // Duplicate Return statement within the same scope, every scope should only have one return value
        assert(std::find(return_scopes.begin(), return_scopes.end(), scope->scope_id) == return_scopes.end());
        return_scopes.push_back(scope->scope_id);
    }
    if (const GroupExpressionNode *group_node = dynamic_cast<const GroupExpressionNode *>(expr.value().get())) {
        for (auto &group_expr : group_node->expressions) {
            if (const VariableNode *variable_node = dynamic_cast<const VariableNode *>(group_expr.get())) {
                std::vector<unsigned int> &return_scopes = std::get<5>(scope->variables.at(variable_node->name));
                // Duplicate Return statement within the same scope, every scope should only have one return value
                assert(std::find(return_scopes.begin(), return_scopes.end(), scope->scope_id) == return_scopes.end());
                return_scopes.push_back(scope->scope_id);
            }
        }
    }
    if (expr.value()->type->to_string() != return_type->to_string()) {
        // Check for implicit castability, if not implicitely castable, throw an error
        std::optional<bool> castability = check_castability(return_type, expr.value()->type);
        if (!castability.has_value() || !castability.value()) {
            // Its either not implicitely castable or only castable in the direction return_type -> expr_type
            // but for the return expression to be implicitely castable we need the direction expr_type -> return_type
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        expr = std::make_unique<TypeCastNode>(return_type, expr.value());
    }
    return_expr = std::move(expr.value());
    return ReturnNode(return_expr);
}

std::optional<std::unique_ptr<IfNode>> Parser::create_if(std::shared_ptr<Scope> scope,
    std::vector<std::pair<token_slice, std::vector<Line>>> &if_chain) {
    assert(!if_chain.empty());
    std::pair<token_slice, std::vector<Line>> this_if_pair = if_chain.front();
    if_chain.erase(if_chain.begin());

    bool has_if = false;
    bool has_else = false;
    // Remove everything in front of the expression (\n, \t, else, if)
    for (auto it = this_if_pair.first.first; it != this_if_pair.first.second; ++it) {
        if (it->token == TOK_ELSE) {
            has_else = true;
        } else if (it->token == TOK_IF) {
            has_if = true;
            this_if_pair.first.first++;
            break;
        }
        this_if_pair.first.first++;
    }
    // Remove everything after the expression (:, \n)
    for (auto rev_it = std::prev(this_if_pair.first.second); rev_it != this_if_pair.first.first; --rev_it) {
        if (rev_it->token == TOK_COLON) {
            this_if_pair.first.second--;
            break;
        }
        this_if_pair.first.second--;
    }
    assert(!has_else || has_if);

    // Create the if statements condition and body statements
    std::optional<std::unique_ptr<ExpressionNode>> condition = create_expression( //
        scope, this_if_pair.first, Type::get_primitive_type("bool")               //
    );
    if (!condition.has_value()) {
        // Invalid expression inside if statement
        return std::nullopt;
    }
    if (condition.value()->type->to_string() != "bool") {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, this_if_pair.first, Type::get_primitive_type("bool"),
            condition.value()->type);
        return std::nullopt;
    }
    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(scope);
    auto body_statements = create_body(body_scope, this_if_pair.second);
    if (!body_statements.has_value()) {
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());
    std::optional<std::variant<std::unique_ptr<IfNode>, std::shared_ptr<Scope>>> else_scope = std::nullopt;

    // Check if the chain contains any values (more if blocks in the chain) and parse them accordingly
    if (!if_chain.empty()) {
        if (Matcher::tokens_contain(if_chain.front().first, Matcher::token(TOK_IF))) {
            // 'else if'
            else_scope = create_if(scope, if_chain);
        } else {
            // 'else'
            if (if_chain.size() > 1) {
                // Dangling else or else if statement after last else statement
                THROW_ERR(ErrStmtDanglingElse, ERR_PARSING, file_name, if_chain.at(1).first);
                return std::nullopt;
            }
            std::shared_ptr<Scope> else_scope_ptr = std::make_shared<Scope>(scope);
            auto else_body_statements = create_body(else_scope_ptr, if_chain.front().second);
            if (!else_body_statements.has_value()) {
                return std::nullopt;
            }
            else_scope_ptr->body = std::move(else_body_statements.value());
            else_scope = std::move(else_scope_ptr);
        }
    }

    return std::make_unique<IfNode>(condition.value(), body_scope, else_scope);
}

std::optional<std::unique_ptr<WhileNode>> Parser::create_while_loop( //
    std::shared_ptr<Scope> scope,                                    //
    const token_slice &definition,                                   //
    const std::vector<Line> &body                                    //
) {
    token_slice condition_tokens = definition;
    // Remove everything in front of the expression (\n, \t, else, if)
    for (auto it = condition_tokens.first; it != condition_tokens.second; ++it) {
        if (it->token == TOK_WHILE) {
            condition_tokens.first++;
            break;
        }
        condition_tokens.first++;
    }
    // Remove everything after the expression (:, \n)
    for (auto rev_it = std::prev(condition_tokens.second); rev_it != condition_tokens.first; --rev_it) {
        if (rev_it->token == TOK_COLON) {
            condition_tokens.second--;
            break;
        }
        condition_tokens.second--;
    }

    std::optional<std::unique_ptr<ExpressionNode>> condition = create_expression(scope, condition_tokens, Type::get_primitive_type("bool"));
    if (!condition.has_value()) {
        // Invalid expression inside while statement
        return std::nullopt;
    }

    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(scope);
    auto body_statements = create_body(body_scope, body);
    if (!body_statements.has_value()) {
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());
    std::unique_ptr<WhileNode> while_node = std::make_unique<WhileNode>(condition.value(), body_scope);
    return while_node;
}

std::optional<std::unique_ptr<ForLoopNode>> Parser::create_for_loop( //
    std::shared_ptr<Scope> scope,                                    //
    const token_slice &definition,                                   //
    const std::vector<Line> &body                                    //
) {
    std::optional<std::unique_ptr<StatementNode>> initializer;
    std::optional<std::unique_ptr<ExpressionNode>> condition;
    std::optional<std::unique_ptr<StatementNode>> looparound;

    // Get the content of the for loop
    std::optional<uint2> expressions_range = Matcher::get_next_match_range(definition, Matcher::until_colon);
    if (!expressions_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Get the actual expressions inside the for loops definition
    std::vector<uint2> expression_ranges = Matcher::get_match_ranges_in_range( //
        definition, Matcher::until_semicolon, expressions_range.value()        //
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
    std::shared_ptr<Scope> definition_scope = std::make_shared<Scope>(scope);
    uint2 &initializer_range = expression_ranges.at(0);
    // "remove" everything including the 'for' keyword from the initializer statement
    // otherwise the for loop would create itself recursively
    for (unsigned int i = initializer_range.first; i < initializer_range.second; i++) {
        if ((definition.first + i)->token == TOK_FOR) {
            initializer_range.first = i + 1;
            break;
        }
    }
    // Parse the initializer statement
    token_slice initializer_tokens = {definition.first + initializer_range.first, definition.first + initializer_range.second};
    initializer = create_statement(definition_scope, initializer_tokens);
    if (!initializer.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    definition_scope->body.emplace_back(std::move(initializer.value()));

    // Parse the loop condition expression
    uint2 &condition_range = expression_ranges.at(1);
    const token_slice condition_tokens = {definition.first + condition_range.first, definition.first + condition_range.second};
    condition = create_expression(definition_scope, condition_tokens);
    if (!condition.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Parse the for loops body
    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(definition_scope);
    auto body_statements = create_body(body_scope, body);
    if (!body_statements.has_value()) {
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());

    // Parse the looparound statement. The looparound statement is actually part of the body is the last statement of the body
    token_slice looparound_tokens = {definition.first + condition_range.second, definition.first + expressions_range.value().second};
    looparound = create_statement(body_scope, looparound_tokens);
    if (!looparound.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return std::make_unique<ForLoopNode>(condition.value(), definition_scope, looparound.value(), body_scope);
}

std::optional<std::unique_ptr<EnhForLoopNode>> Parser::create_enh_for_loop( //
    std::shared_ptr<Scope> scope,                                           //
    const token_slice &definition,                                          //
    const std::vector<Line> &body                                           //
) {
    token_slice definition_mut = definition;
    remove_trailing_garbage(definition_mut);

    // Now the first token should be the `for` token
    assert(definition_mut.first->token == TOK_FOR);
    definition_mut.first++;

    // The next token should either be a `(` or an identifer. If its an identifier we use the "tupled" enhanced for loop approach
    std::variant<std::pair<std::optional<std::string>, std::optional<std::string>>, std::string> iterators;
    if (definition_mut.first->token == TOK_IDENTIFIER) {
        // Its a tuple, e.g. `for t in iterable:` where `t` is of type `data<u64, T>`
        iterators = std::string(definition_mut.first->lexme);
        definition_mut.first++;
    } else {
        // Its a group, e.g. `for (index, element) in iterable:`
        assert(definition_mut.first->token == TOK_LEFT_PAREN);
        definition_mut.first++;
        std::optional<std::string> index_identifier;
        if (definition_mut.first->token == TOK_IDENTIFIER) {
            index_identifier = std::string(definition_mut.first->lexme);
        }
        definition_mut.first++;
        assert(definition_mut.first->token == TOK_COMMA);
        definition_mut.first++;
        std::optional<std::string> element_identifier;
        if (definition_mut.first->token == TOK_IDENTIFIER) {
            element_identifier = std::string(definition_mut.first->lexme);
        }
        definition_mut.first++;
        assert(definition_mut.first->token == TOK_RIGHT_PAREN);
        definition_mut.first++;
        iterators = std::make_pair(index_identifier, element_identifier);
    }
    assert(definition_mut.first->token == TOK_IN);
    definition_mut.first++;

    // Create the definition scope
    std::shared_ptr<Scope> definition_scope = std::make_shared<Scope>(scope);

    // The rest of the definition is the iterable expression
    std::optional<std::unique_ptr<ExpressionNode>> iterable = create_expression(definition_scope, definition_mut);
    if (!iterable.has_value()) {
        return std::nullopt;
    }
    // Now that the iterable is parsed we know its type and we can check if its an iterable. For now, only arrays and strings are considered
    // as being iterable
    std::shared_ptr<Type> element_type;
    if (const PrimitiveType *primitive_type = dynamic_cast<const PrimitiveType *>(iterable.value()->type.get())) {
        if (primitive_type->type_name != "str") {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        element_type = Type::get_primitive_type("u8");
    } else if (const ArrayType *array_type = dynamic_cast<const ArrayType *>(iterable.value()->type.get())) {
        element_type = array_type->type;
    } else {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Add the variable(s) to the definition scope
    if (std::holds_alternative<std::string>(iterators)) {
        // We add the tuple variable to the definition scope
        const std::string tuple_name = std::get<std::string>(iterators);
        std::vector<std::shared_ptr<Type>> tuple_types = {Type::get_primitive_type("u64"), element_type};
        std::shared_ptr<Type> tuple_type = std::make_shared<TupleType>(tuple_types);
        if (!Type::add_type(tuple_type)) {
            tuple_type = Type::get_type_from_str(tuple_type->to_string()).value();
        }
        if (!definition_scope->add_variable(tuple_name, tuple_type, definition_scope->scope_id, false, true, true)) {
            auto tuple_it = definition_mut.first - 2;
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, tuple_it->line, tuple_it->column, tuple_name);
            return std::nullopt;
        }
    } else {
        // We add the index and element variable to the definition scope
        std::shared_ptr<Type> index_type = Type::get_primitive_type("u64");
        const auto its = std::get<std::pair<std::optional<std::string>, std::optional<std::string>>>(iterators);
        const std::optional<std::string> index_name = its.first;
        const std::optional<std::string> element_name = its.second;
        if (index_name.has_value()) {
            auto index_it = definition_mut.first - 5;
            if (!definition_scope->add_variable(index_name.value(), index_type, definition_scope->scope_id, false, false, true)) {
                THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, index_it->line, index_it->column, index_name.value());
                return std::nullopt;
            }
        }
        if (element_name.has_value()) {
            auto element_it = definition_mut.first - 3;
            if (!definition_scope->add_variable(element_name.value(), element_type, definition_scope->scope_id, true, false, true)) {
                THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, element_it->line, element_it->column, element_name.value());
                return std::nullopt;
            }
        }
    }

    // Now create the body scope and parse the body
    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(definition_scope);
    auto body_statements = create_body(body_scope, body);
    if (!body_statements.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());

    return std::make_unique<EnhForLoopNode>(iterators, iterable.value(), definition_scope, body_scope);
}

bool Parser::create_switch_branch_body(                              //
    std::shared_ptr<Scope> scope,                                    //
    std::vector<std::unique_ptr<ExpressionNode>> &match_expressions, //
    std::vector<SSwitchBranch> &s_branches,                          //
    std::vector<ESwitchBranch> &e_branches,                          //
    std::vector<Line>::const_iterator &line_it,                      //
    const std::vector<Line> &body,                                   //
    const token_slice &tokens,                                       //
    const uint2 &match_range,                                        //
    const bool is_statement                                          //
) {
    if (!is_statement) {
        // When it's a switch expression, no body will follow, ever. Only expressions are allowed to the right of the arrow, so we
        // can parse the rhs as an expression
        assert(std::prev(tokens.second)->token == TOK_SEMICOLON);
        const token_slice expression_tokens = {tokens.first + match_range.second, tokens.second - 1};
        auto expression = create_expression(scope, expression_tokens);
        if (!expression.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        e_branches.emplace_back(scope, match_expressions, expression.value());
        ++line_it;
        return true;
    }
    // Check if the colon is the last symbol in this line. If it is, a body follows. If after the colon something is written the
    // "body" is a single statement written directly after the colon.
    std::shared_ptr<Scope> branch_body = std::make_shared<Scope>(scope);
    if (tokens.first + match_range.second != tokens.second) {
        // A single statement follows, this means that the line needs to end with a semicolon
        const token_slice statement_tokens = {tokens.first + match_range.second, tokens.second};
        if (std::prev(statement_tokens.second)->token != TOK_SEMICOLON) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        auto statement = create_statement(branch_body, statement_tokens);
        if (!statement.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        branch_body->body.push_back(std::move(statement.value()));
        s_branches.emplace_back(match_expressions, branch_body);
        ++line_it;
        return true;
    }
    // A "normal" body follows. Each line of the body needs to start with a definition, e.g. end with a colon.
    assert(tokens.first + match_range.second == tokens.second);
    assert(std::prev(tokens.second)->token == TOK_COLON);
    const unsigned int case_indent_lvl = line_it->indent_lvl;
    auto body_start = ++line_it;
    while (line_it != body.end() && line_it->indent_lvl > case_indent_lvl) {
        ++line_it;
    }
    if (body_start == line_it) {
        // No body found
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    const std::vector<Line> body_lines(body_start, line_it);
    auto body_statements = create_body(branch_body, body_lines);
    if (!body_statements.has_value()) {
        return false;
    }
    branch_body->body = std::move(body_statements.value());
    s_branches.emplace_back(match_expressions, branch_body);
    return true;
}

bool Parser::create_switch_branches(            //
    std::shared_ptr<Scope> scope,               //
    std::vector<SSwitchBranch> &s_branches,     //
    std::vector<ESwitchBranch> &e_branches,     //
    const std::vector<Line> &body,              //
    const std::shared_ptr<Type> &switcher_type, //
    const bool is_statement                     //
) {
    // Okay now parse the "body" of the switch statement. It's body follows a clear guideline, namely that everything to the left of the
    // `:` is considered an expression which has to be matched, and the type of the expression to the left needs to match the type of
    // the switcher expression. At least that's how i will handle it now. Also, only certain types of expressions are allowed at all for
    // enums, for now thats only "literals", e.g. `EnumType.VALUE`
    for (auto line_it = body.begin(); line_it != body.end();) {
        const token_slice &tokens = line_it->tokens;
        // First, get all tokens until the colon to be able to parse the matching expression
        std::optional<uint2> match_range = std::nullopt;
        if (is_statement) {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_colon);
        } else {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_arrow);
        }
        if (!match_range.has_value() || match_range.value().first != 0) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        token_slice match_tokens = {tokens.first, tokens.first + match_range.value().second - 1};
        std::vector<std::unique_ptr<ExpressionNode>> matches;
        if (std::next(match_tokens.first) == match_tokens.second && match_tokens.first->token == TOK_ELSE) {
            matches.emplace_back(std::make_unique<DefaultNode>(switcher_type));
        } else {
            auto match_expressions = create_group_expressions(scope, match_tokens);
            if (!match_expressions.has_value()) {
                return false;
            }
            matches = std::move(match_expressions.value());
        }
        if (matches.empty()) {
            return false;
        }
        if (dynamic_cast<const PrimitiveType *>(switcher_type.get())) {
            for (const auto &match : matches) {
                const bool is_literal = dynamic_cast<const LiteralNode *>(match.get()) != nullptr;
                const bool is_default_value = dynamic_cast<const DefaultNode *>(match.get()) != nullptr;
                if (!is_literal && !is_default_value) {
                    // Not allowed value for the switch statement's expression
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
            }
        } else {
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return false;
        }
        if (!create_switch_branch_body(                                                                           //
                scope, matches, s_branches, e_branches, line_it, body, tokens, match_range.value(), is_statement) //
        ) {
            return false;
        }
    }
    return true;
}

bool Parser::create_enum_switch_branches(       //
    std::shared_ptr<Scope> scope,               //
    std::vector<SSwitchBranch> &s_branches,     //
    std::vector<ESwitchBranch> &e_branches,     //
    const std::vector<Line> &body,              //
    const std::shared_ptr<Type> &switcher_type, //
    const EnumNode *enum_node,                  //
    const bool is_statement                     //
) {
    // First, we check for all the matches. All matches *must* be identifiers, where each identifier matches one value of the enum. Each
    // identifier can only be used once in the switch. If there exists a default branch (the else keyword) then it is not allowed that all
    // other enum values are matched, as the else branch would then effectively become unreachable.
    std::vector<std::string> matched_enum_values;
    const std::vector<std::string> &enum_values = enum_node->values;
    bool is_default_present = false;
    matched_enum_values.reserve(enum_values.size());
    for (auto line_it = body.begin(); line_it != body.end();) {
        const token_slice &tokens = line_it->tokens;
        // First, get all tokens until the colon to be able to parse the matching expression
        std::optional<uint2> match_range = std::nullopt;
        if (is_statement) {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_colon);
        } else {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_arrow);
        }
        if (!match_range.has_value() || match_range.value().first != 0) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const token_slice match_tokens = {tokens.first, tokens.first + match_range.value().second - 1};
        std::vector<std::unique_ptr<ExpressionNode>> match_expressions;
        if (std::next(match_tokens.first) == match_tokens.second) {
            // We only have one value match in here
            if (match_tokens.first->token == TOK_TYPE) {
                // We need to access the `lexme` field of the token so a type token would cause a crash here
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            if (match_tokens.first->token == TOK_ELSE) {
                // The else branch, which gets matched if no other branch get's matched
                is_default_present = true;
                match_expressions.push_back(std::make_unique<DefaultNode>(switcher_type));
            } else {
                const std::string enum_value(match_tokens.first->lexme);
                std::vector<std::string>::const_iterator enum_id = std::find(          //
                    matched_enum_values.begin(), matched_enum_values.end(), enum_value //
                );
                if (enum_id != matched_enum_values.end()) {
                    // Duplicate branch enum
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                enum_id = std::find(enum_values.begin(), enum_values.end(), enum_value);
                if (enum_id == enum_values.end()) {
                    // Enum value not part of the enum values
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                matched_enum_values.push_back(enum_value);
                LitValue lit_value = LitEnum{switcher_type, std::vector<std::string>{enum_value}};
                match_expressions.push_back(std::make_unique<LiteralNode>(lit_value, switcher_type));
            }
        } else {
            // There could be multiple values here
            for (auto it = match_tokens.first; it != match_tokens.second; ++it) {
                if (it->token == TOK_COMMA) {
                    continue;
                }
                if (it->token != TOK_IDENTIFIER) {
                    // Default branches are not allowed in a branch that has multiple values leading to it
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                const std::string enum_value(it->lexme);
                std::vector<std::string>::const_iterator enum_id = std::find(          //
                    matched_enum_values.begin(), matched_enum_values.end(), enum_value //
                );
                if (enum_id != matched_enum_values.end()) {
                    // Duplicate branch enum
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                enum_id = std::find(enum_values.begin(), enum_values.end(), enum_value);
                if (enum_id == enum_values.end()) {
                    // Enum value not part of the enum values
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                matched_enum_values.push_back(enum_value);
                LitValue lit_value = LitEnum{switcher_type, std::vector<std::string>{enum_value}};
                match_expressions.push_back(std::make_unique<LiteralNode>(lit_value, switcher_type));
            }
        }
        if (!create_switch_branch_body(                                                                                     //
                scope, match_expressions, s_branches, e_branches, line_it, body, tokens, match_range.value(), is_statement) //
        ) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    }
    if (is_default_present ^ (matched_enum_values.size() != enum_values.size())) {
        // Either we have a default branch and all enum values are matched
        // Or we don't have a default branch and not all enum values are matched
        // Either way, we don't have a branch for every possible value of the enum, so that's an error
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    return true;
}

bool Parser::create_error_switch_branches(      //
    std::shared_ptr<Scope> scope,               //
    std::vector<SSwitchBranch> &s_branches,     //
    std::vector<ESwitchBranch> &e_branches,     //
    const std::vector<Line> &body,              //
    const std::shared_ptr<Type> &switcher_type, //
    const ErrorNode *error_node,                //
    const bool is_statement                     //
) {
    // First, we check for all the matches. All matches *must* be identifiers, where each identifier matches one value of the error set.
    // Each identifier can only be used once in the switch. If there exists a default branch (the else keyword) then it is not allowed that
    // all other error set values are matched, as the else branch would then effectively become unreachable.
    std::vector<unsigned int> matched_ids;
    bool is_default_present = false;
    for (auto line_it = body.begin(); line_it != body.end();) {
        const token_slice &tokens = line_it->tokens;
        // First, get all tokens until the colon to be able to parse the matching expression
        std::optional<uint2> match_range = std::nullopt;
        if (is_statement) {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_colon);
        } else {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_arrow);
        }
        if (!match_range.has_value() || match_range.value().first != 0) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const token_slice match_tokens = {tokens.first, tokens.first + match_range.value().second - 1};
        std::vector<std::unique_ptr<ExpressionNode>> match_expressions;
        if (std::next(match_tokens.first) == match_tokens.second) {
            // We only have one value match in here
            if (match_tokens.first->token == TOK_TYPE) {
                // We need to access the `lexme` field of the token so a type token would cause a crash here
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            if (match_tokens.first->token == TOK_ELSE) {
                // The else branch, which gets matched if no other branch get's matched
                is_default_present = true;
                match_expressions.push_back(std::make_unique<DefaultNode>(switcher_type));
            } else {
                const std::string error_value(match_tokens.first->lexme);
                auto pair_maybe = error_node->get_id_msg_pair_of_value(error_value);
                if (!pair_maybe.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                const unsigned int value_id = pair_maybe.value().first;
                if (std::find(matched_ids.begin(), matched_ids.end(), value_id) != matched_ids.end()) {
                    // Duplicate value in switch
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                matched_ids.push_back(value_id);
                LitValue lit_value = LitError{switcher_type, error_value, std::nullopt};
                match_expressions.push_back(std::make_unique<LiteralNode>(lit_value, switcher_type));
            }
        } else {
            // There could be multiple values here
            for (auto it = match_tokens.first; it != match_tokens.second; ++it) {
                if (it->token == TOK_COMMA) {
                    continue;
                }
                if (it->token != TOK_IDENTIFIER) {
                    // Default branches are not allowed in a branch that has multiple values leading to it
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                const std::string error_value(it->lexme);
                auto pair_maybe = error_node->get_id_msg_pair_of_value(error_value);
                if (!pair_maybe.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                const unsigned int value_id = pair_maybe.value().first;
                if (std::find(matched_ids.begin(), matched_ids.end(), value_id) != matched_ids.end()) {
                    // Duplicate value in switch
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                matched_ids.push_back(value_id);
                LitValue lit_value = LitError{switcher_type, error_value, std::nullopt};
                match_expressions.push_back(std::make_unique<LiteralNode>(lit_value, switcher_type));
            }
        }
        if (!create_switch_branch_body(                                                                                     //
                scope, match_expressions, s_branches, e_branches, line_it, body, tokens, match_range.value(), is_statement) //
        ) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    }
    const unsigned int value_count = error_node->get_value_count();
    if (is_default_present ^ (matched_ids.size() != value_count)) {
        // Either we have a default branch and all error values are matched
        // Or we don't have a default branch and not all error values are matched
        // Either way, we don't have a branch for every possible value of the error set, so that's an error
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    return true;
}

bool Parser::create_optional_switch_branches(   //
    std::shared_ptr<Scope> scope,               //
    std::vector<SSwitchBranch> &s_branches,     //
    std::vector<ESwitchBranch> &e_branches,     //
    const std::vector<Line> &body,              //
    const std::shared_ptr<Type> &switcher_type, //
    const bool is_statement,                    //
    const bool is_mutable                       //
) {
    // We simply check for the `none` and the `identifier` branches, as those are the only possible branches
    bool none_branch_parsed = false;
    bool value_branch_parsed = false;
    for (auto line_it = body.begin(); line_it != body.end();) {
        const token_slice &tokens = line_it->tokens;
        // First, get all tokens until the colon to be able to parse the matching expression
        std::optional<uint2> match_range = std::nullopt;
        if (is_statement) {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_colon);
        } else {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_arrow);
        }
        if (!match_range.has_value() || match_range.value().first != 0) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const token_slice match_tokens = {tokens.first, tokens.first + match_range.value().second - 1};
        std::vector<std::unique_ptr<ExpressionNode>> match_expressions;
        if (match_tokens.first->token == TOK_NONE) {
            // It's the none literal
            if (none_branch_parsed) {
                // Duplicate none branch
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            LitValue lit_value = LitOptional{};
            match_expressions.push_back(std::make_unique<LiteralNode>(lit_value, switcher_type));
            if (!create_switch_branch_body(                                                                                     //
                    scope, match_expressions, s_branches, e_branches, line_it, body, tokens, match_range.value(), is_statement) //
            ) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            none_branch_parsed = true;
        } else if (match_tokens.first->token == TOK_IDENTIFIER) {
            // It's the optional extraction through which we can access the "real" value of the optional through this new variable
            if (value_branch_parsed) {
                // Duplicate value branch
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            const OptionalType *optional_type = dynamic_cast<const OptionalType *>(switcher_type.get());
            const std::string match_str(match_tokens.first->lexme);
            match_expressions.push_back(std::make_unique<SwitchMatchNode>(optional_type->base_type, match_str, 1));
            std::shared_ptr<Scope> branch_scope = std::make_shared<Scope>(scope);
            const unsigned int scope_id = branch_scope->scope_id;
            const std::string var_name(match_tokens.first->lexme);
            if (!branch_scope->add_variable(var_name, optional_type->base_type, scope_id, is_mutable, false, true)) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            if (!create_switch_branch_body(                                   //
                    branch_scope, match_expressions, s_branches, e_branches,  //
                    line_it, body, tokens, match_range.value(), is_statement) //
            ) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            value_branch_parsed = true;
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    }
    return true;
}

bool Parser::create_variant_switch_branches(    //
    std::shared_ptr<Scope> scope,               //
    std::vector<SSwitchBranch> &s_branches,     //
    std::vector<ESwitchBranch> &e_branches,     //
    const std::vector<Line> &body,              //
    const std::shared_ptr<Type> &switcher_type, //
    const bool is_statement,                    //
    const bool is_mutable                       //
) {
    // We check for each type as an index to check in which branch we switch to, and then we can directly branch in the switch depending on
    // which type index the variant holds, pretty simple actually
    std::vector<int> branch_indices;
    const VariantType *variant_type = dynamic_cast<const VariantType *>(switcher_type.get());
    assert(variant_type != nullptr);
    const auto &possible_types = variant_type->get_possible_types();

    // A match can either be tagged or untagged. For now, we only focus on untagged variants, so the "match" must be a type followed by
    // '(IDENTIFIER)' where, similar to the optional switch, the 'value' of the variant is directly accessible through that identifier as a
    // "pointer"
    bool is_default_present = false;
    for (auto line_it = body.begin(); line_it != body.end();) {
        const token_slice &tokens = line_it->tokens;
        // First, get all tokens until the colon to be able to parse the matching expression
        std::optional<uint2> match_range = std::nullopt;
        if (is_statement) {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_colon);
        } else {
            match_range = Matcher::get_next_match_range(tokens, Matcher::until_arrow);
        }
        if (!match_range.has_value() || match_range.value().first != 0) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        std::vector<std::unique_ptr<ExpressionNode>> match_expressions;
        token_slice match_tokens = {tokens.first, tokens.first + match_range.value().second - 1};
        if (match_tokens.first->token != TOK_TYPE) {
            if (match_tokens.first->token == TOK_ELSE) {
                // Default branch
                if (std::find(branch_indices.begin(), branch_indices.end(), -1) != branch_indices.end()) {
                    // Duplicate default block
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                match_expressions.push_back(std::make_unique<DefaultNode>(switcher_type));
                if (!create_switch_branch_body(                                                                                     //
                        scope, match_expressions, s_branches, e_branches, line_it, body, tokens, match_range.value(), is_statement) //
                ) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                branch_indices.push_back(-1);
                continue;
            }
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        auto type_it = possible_types.begin();

        if (match_tokens.first->type == switcher_type) {
            // It's a tagged variation
            match_tokens.first++;
            if (match_tokens.first->token != TOK_DOT) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            match_tokens.first++;
            std::string tag = "";
            if (match_tokens.first->token == TOK_IDENTIFIER) {
                tag = match_tokens.first->lexme;
            } else if (match_tokens.first->token == TOK_TYPE) {
                tag = match_tokens.first->type->to_string();
            } else {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            for (; type_it != possible_types.end(); ++type_it) {
                if (type_it->first == tag) {
                    break;
                }
            }
        } else {
            // It's not tagged
            for (; type_it != possible_types.end(); ++type_it) {
                if (type_it->second == match_tokens.first->type && !type_it->first.has_value()) {
                    break;
                }
            }
        }
        if (type_it == possible_types.end()) {
            // Unsupported type in variant switch
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const unsigned int type_idx = 1 + std::distance(possible_types.begin(), type_it);
        if (std::find(branch_indices.begin(), branch_indices.end(), type_idx) != branch_indices.end()) {
            // Duplicate type
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        branch_indices.push_back(type_idx);

        // Then `(`, `IDENTIFIER`, `)` must follow
        if ((match_tokens.first + 1)->token != TOK_LEFT_PAREN) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        if ((match_tokens.first + 2)->token != TOK_IDENTIFIER) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        if ((match_tokens.first + 3)->token != TOK_RIGHT_PAREN) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const std::string access_name((match_tokens.first + 2)->lexme);
        const std::shared_ptr<Type> &access_type = possible_types.at(type_idx - 1).second;
        match_expressions.push_back(std::make_unique<SwitchMatchNode>(access_type, access_name, type_idx));

        std::shared_ptr<Scope> branch_scope = std::make_shared<Scope>(scope);
        if (!branch_scope->add_variable(access_name, access_type, branch_scope->scope_id, is_mutable, false, true)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        if (!create_switch_branch_body(                                                                                            //
                branch_scope, match_expressions, s_branches, e_branches, line_it, body, tokens, match_range.value(), is_statement) //
        ) {
            return false;
        }
    }
    if (is_default_present ^ (branch_indices.size() != possible_types.size())) {
        // Either we have a default branch and all variant types are matched
        // Or we don't have a default branch and not all variant types are matched
        // Either way, we don't have a branch for every possible type of the variant, so that's an error
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    return true;
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_switch_statement( //
    std::shared_ptr<Scope> scope,                                              //
    const token_slice &definition,                                             //
    const std::vector<Line> &body                                              //
) {
    // First, check if the definition starts with a switch token. If it starts with a switch token it's a switch statement, otherwise
    // it's a switch expression
    token_slice switcher_tokens = definition;
    const bool is_statement = switcher_tokens.first->token == TOK_SWITCH;
    assert(std::prev(switcher_tokens.second)->token == TOK_COLON);
    switcher_tokens.second--;
    if (is_statement) {
        switcher_tokens.first++;
    } else {
        // The switcher expression is everything in between the switch token to the colon
        for (; switcher_tokens.first->token != TOK_SWITCH; ++switcher_tokens.first) {}
        switcher_tokens.first++;
    }
    // The rest of the definition is the switcher expression
    std::vector<SSwitchBranch> s_branches;
    std::vector<ESwitchBranch> e_branches;
    std::optional<std::unique_ptr<ExpressionNode>> switcher = create_expression(scope, switcher_tokens);
    if (!switcher.has_value()) {
        return std::nullopt;
    }
    if (const EnumType *enum_type = dynamic_cast<const EnumType *>(switcher.value()->type.get())) {
        if (!create_enum_switch_branches(scope, s_branches, e_branches, body, switcher.value()->type, enum_type->enum_node, is_statement)) {
            return std::nullopt;
        }
    } else if (dynamic_cast<const OptionalType *>(switcher.value()->type.get())) {
        const VariableNode *var_node = dynamic_cast<const VariableNode *>(switcher.value().get());
        assert(var_node != nullptr);
        const bool is_mutable = std::get<2>(scope->variables.find(var_node->name)->second);
        if (!create_optional_switch_branches(scope, s_branches, e_branches, body, switcher.value()->type, is_statement, is_mutable)) {
            return std::nullopt;
        }
    } else if (dynamic_cast<const VariantType *>(switcher.value()->type.get())) {
        const VariableNode *var_node = dynamic_cast<const VariableNode *>(switcher.value().get());
        assert(var_node != nullptr);
        const bool is_mutable = std::get<2>(scope->variables.find(var_node->name)->second);
        if (!create_variant_switch_branches(scope, s_branches, e_branches, body, switcher.value()->type, is_statement, is_mutable)) {
            return std::nullopt;
        }
    } else if (const ErrorSetType *error_type = dynamic_cast<const ErrorSetType *>(switcher.value()->type.get())) {
        const ErrorNode *error_node = error_type->error_node;
        if (!create_error_switch_branches(scope, s_branches, e_branches, body, switcher.value()->type, error_node, is_statement)) {
            return std::nullopt;
        }
    } else {
        if (!create_switch_branches(scope, s_branches, e_branches, body, switcher.value()->type, is_statement)) {
            return std::nullopt;
        }
    }
    if (is_statement) {
        return std::make_unique<SwitchStatement>(switcher.value(), s_branches);
    }
    // Because it's a statement which contains the switch expression as it's rhs, we still need to parse everything to the left of the
    // switch and pass the switch as the rhs expression to it.
    assert(!e_branches.empty());
    // Check if all branch expressions share the same type, throw an error if they are not
    std::shared_ptr<Type> expr_type = e_branches.front().expr->type;
    for (const auto &branch : e_branches) {
        if (branch.expr->type != expr_type) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }
    auto switch_expr = std::make_unique<SwitchExpression>(switcher.value(), e_branches);
    // Now we need to parse the lhs of the switch *somehow*...
    token_slice lhs_tokens = {definition.first, switcher_tokens.first - 1};
    assert(lhs_tokens.second->token == TOK_SWITCH);
    auto whole_statement = create_statement(scope, lhs_tokens, std::move(switch_expr));
    if (!whole_statement.has_value()) {
        return std::nullopt;
    }
    return whole_statement;
}

std::optional<std::unique_ptr<CatchNode>> Parser::create_catch( //
    std::shared_ptr<Scope> scope,                               //
    const token_slice &definition,                              //
    const std::vector<Line> &body,                              //
    std::vector<std::unique_ptr<StatementNode>> &statements     //
) {
    // First, extract everything left of the 'catch' statement and parse it as a normal (unscoped) statement
    std::optional<token_list::iterator> catch_id = std::nullopt;
    for (auto it = definition.first; it != definition.second; ++it) {
        if (it->token == TOK_CATCH) {
            catch_id = it;
            break;
        }
    }
    assert(catch_id.has_value());
    // A call is three tokens minimum: identifier(), so everything smaller than that means the catch stands alone
    if (catch_id.value() < definition.first + 3) {
        THROW_ERR(ErrStmtDanglingCatch, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }

    token_slice left_of_catch = {definition.first, catch_id.value()};
    std::optional<std::unique_ptr<StatementNode>> lhs = create_statement(scope, left_of_catch);
    if (!lhs.has_value()) {
        return std::nullopt;
    }
    statements.emplace_back(std::move(lhs.value()));
    // Get the last parsed call and set the 'has_catch' property of the call node
    if (!last_parsed_call.has_value()) {
        THROW_ERR(ErrStmtDanglingCatch, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }
    CallNodeBase *catch_base_call = last_parsed_call.value();
    catch_base_call->has_catch = true;

    const token_slice right_of_catch = {catch_id.value(), std::prev(definition.second)};
    std::optional<std::string> err_var = std::nullopt;
    for (auto it = right_of_catch.first; it != right_of_catch.second; ++it) {
        if (it->token == TOK_CATCH && std::next(it) != right_of_catch.second && std::next(it)->token == TOK_IDENTIFIER) {
            err_var = std::next(it)->lexme;
        }
    }

    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(scope);
    if (err_var.has_value()) {
        // Get all the possible error types of the call and give the error value a type depending on them
        const auto &error_types = catch_base_call->error_types;
        std::shared_ptr<Type> err_variable_type;
        if (error_types.size() == 1) {
            err_variable_type = Type::get_primitive_type("anyerror");
        } else {
            const std::variant<VariantNode *const, std::vector<std::shared_ptr<Type>>> var_or_list = error_types;
            err_variable_type = std::make_shared<VariantType>(var_or_list, true);
            if (!Type::add_type(err_variable_type)) {
                err_variable_type = Type::get_type_from_str(err_variable_type->to_string()).value();
            }
        }
        if (!body_scope->add_variable(err_var.value(), err_variable_type, body_scope->scope_id, false, false)) {
            THROW_ERR(                                                                                                                //
                ErrVarRedefinition, ERR_PARSING, file_name, right_of_catch.first->line, right_of_catch.first->column, err_var.value() //
            );
        }
        auto body_statements = create_body(body_scope, body);
        if (!body_statements.has_value()) {
            return std::nullopt;
        }
        body_scope->body = std::move(body_statements.value());
    } else {
        // Implicit switch on the "error variable"
        if (catch_base_call->error_types.size() == 1) {
            // Implicit switch not possible on a function returning only the `anyerror` and not other error sets
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        std::vector<SSwitchBranch> s_branches;
        std::vector<ESwitchBranch> e_branches;
        const std::variant<VariantNode *const, std::vector<std::shared_ptr<Type>>> &var_or_list = catch_base_call->error_types;
        std::shared_ptr<Type> switcher_type = std::make_shared<VariantType>(var_or_list, true);
        if (!Type::add_type(switcher_type)) {
            switcher_type = Type::get_type_from_str(switcher_type->to_string()).value();
        }
        const bool inserting_ok = body_scope->add_variable("__flint_value_err", switcher_type, body_scope->scope_id, false, false);
        assert(inserting_ok);
        if (!create_variant_switch_branches(body_scope, s_branches, e_branches, body, switcher_type, true, false)) {
            return std::nullopt;
        }
        std::unique_ptr<ExpressionNode> dummy_switcher = std::make_unique<VariableNode>("__flint_value_err", switcher_type);
        std::unique_ptr<StatementNode> switch_statement = std::make_unique<SwitchStatement>(dummy_switcher, s_branches);
        body_scope->body.push_back(std::move(switch_statement));
    }
    return std::make_unique<CatchNode>(err_var, body_scope, catch_base_call);
}

std::optional<GroupAssignmentNode> Parser::create_group_assignment( //
    std::shared_ptr<Scope> scope,                                   //
    const token_slice &tokens,                                      //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs             //
) {
    token_slice tokens_mut = tokens;
    assert(tokens_mut.first != tokens_mut.second);
    // Now a left paren is expected as the start of the group assignment
    if (tokens_mut.first->token != TOK_LEFT_PAREN) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Remove the left paren
    tokens_mut.first++;
    // Extract all assignees, we expect assingees to follow the strict pattern of: \( (\W+,)+ \W \)
    // or if you're super correct with the regex it matches this exact pattern: \(\W*(\w+\W*,\W*)+\w+\W*\)
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> assignees;
    unsigned int index = 0;
    for (auto it = tokens_mut.first; it != tokens_mut.second; it += 2) {
        // The next element has to be either a comma or the right paren
        if (std::next(it) == tokens_mut.second || (std::next(it)->token != TOK_COMMA && std::next(it)->token != TOK_RIGHT_PAREN)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const std::string it_lexme(it->lexme);
        // This element is the assignee
        if (scope->variables.find(it_lexme) == scope->variables.end()) {
            THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        if (!std::get<2>(scope->variables.at(it_lexme))) {
            THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        const std::shared_ptr<Type> &expected_type = std::get<0>(scope->variables.at(it_lexme));
        assignees.emplace_back(expected_type, it_lexme);
        index += 2;
        if (std::next(it)->token == TOK_RIGHT_PAREN) {
            break;
        }
    }
    // Erase all the assignment tokens
    tokens_mut.first += index;
    // Now the first token should be an equals token
    if (tokens_mut.first->token != TOK_EQUAL) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Remove the equal sign
    tokens_mut.first++;
    // The rest of the tokens now is the expression
    if (rhs.has_value()) {
        return GroupAssignmentNode(assignees, rhs.value());
    }
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, tokens_mut);
    if (!expr.has_value()) {
        return std::nullopt;
    }
    return GroupAssignmentNode(assignees, expr.value());
}

std::optional<GroupAssignmentNode> Parser::create_group_assignment_shorthand( //
    std::shared_ptr<Scope> scope,                                             //
    const token_slice &tokens,                                                //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                       //
) {
    token_slice tokens_mut = tokens;
    assert(tokens_mut.first != tokens_mut.second);
    // Now a left paren is expected as the start of the group assignment
    if (tokens_mut.first->token != TOK_LEFT_PAREN) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Remove the left paren
    tokens_mut.first++;
    // Extract all assignees, we expect assingees to follow the strict pattern of: \( (\W+,)+ \W \)
    // or if you're super correct with the regex it matches this exact pattern: \(\W*(\w+\W*,\W*)+\w+\W*\)
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> assignees;
    unsigned int index = 0;
    for (auto it = tokens_mut.first; it != tokens_mut.second; it += 2) {
        // The next element has to be either a comma or the right paren
        if (std::next(it) == tokens_mut.second || (std::next(it)->token != TOK_COMMA && std::next(it)->token != TOK_RIGHT_PAREN)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const std::string it_lexme(it->lexme);
        // This element is the assignee
        if (scope->variables.find(it_lexme) == scope->variables.end()) {
            THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        if (!std::get<2>(scope->variables.at(it_lexme))) {
            THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        const std::shared_ptr<Type> &expected_type = std::get<0>(scope->variables.at(it_lexme));
        assignees.emplace_back(expected_type, it_lexme);
        index += 2;
        if (std::next(it)->token == TOK_RIGHT_PAREN) {
            break;
        }
    }
    // Erase all the assignment tokens
    tokens_mut.first += index;

    // Get the operation of the assignment shorthand
    Token operation = TOK_EOF;
    switch (tokens_mut.first->token) {
        case TOK_PLUS_EQUALS:
            operation = TOK_PLUS;
            break;
        case TOK_MINUS_EQUALS:
            operation = TOK_MINUS;
            break;
        case TOK_MULT_EQUALS:
            operation = TOK_MULT;
            break;
        case TOK_DIV_EQUALS:
            operation = TOK_DIV;
            break;
        default:
            assert(false);
            break;
    }
    tokens_mut.first++;

    // The rest of the tokens now is the expression
    std::optional<std::unique_ptr<ExpressionNode>> expr = std::nullopt;
    if (rhs.has_value()) {
        expr = std::move(rhs.value());
    } else {
        expr = create_expression(scope, tokens_mut);
    }
    if (!expr.has_value()) {
        return std::nullopt;
    }

    // The expression now is the group of all assignees within a binop with the rhs expr
    std::vector<std::unique_ptr<ExpressionNode>> lhs_expressions;
    for (const auto &[type, name] : assignees) {
        lhs_expressions.emplace_back(std::make_unique<VariableNode>(name, type));
    }
    std::unique_ptr<ExpressionNode> lhs_expr = std::make_unique<GroupExpressionNode>(lhs_expressions);

    // The "real" expression of the assignment is a binop between the lhs and the "real" expression
    expr = std::make_unique<BinaryOpNode>( //
        operation,                         //
        lhs_expr,                          //
        expr.value(),                      //
        lhs_expr->type,                    //
        true                               //
    );

    return GroupAssignmentNode(assignees, expr.value());
}

std::optional<AssignmentNode> Parser::create_assignment( //
    std::shared_ptr<Scope> scope,                        //
    const token_slice &tokens,                           //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs  //
) {
    token_list toks = clone_from_slice(tokens);
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->token == TOK_IDENTIFIER) {
            const std::string it_lexme(it->lexme);
            if (std::next(it)->token == TOK_EQUAL && ((it + 2) != tokens.second || rhs.has_value())) {
                if (scope->variables.find(it_lexme) == scope->variables.end()) {
                    THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, it->line, it->column, it_lexme);
                    return std::nullopt;
                }
                if (!std::get<2>(scope->variables.at(it_lexme))) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, it->line, it->column, it_lexme);
                    return std::nullopt;
                }
                std::shared_ptr<Type> expected_type = std::get<0>(scope->variables.at(it_lexme));
                if (rhs.has_value()) {
                    if (rhs.value()->type != expected_type) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    return AssignmentNode(expected_type, it_lexme, rhs.value());
                }
                // Parse the expression with the expected type passed into it
                token_slice expression_tokens = {it + 2, tokens.second};
                std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, expression_tokens, expected_type);
                if (!expression.has_value()) {
                    return std::nullopt;
                }
                return AssignmentNode(expected_type, it_lexme, expression.value());
            } else {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<AssignmentNode> Parser::create_assignment_shorthand( //
    std::shared_ptr<Scope> scope,                                  //
    const token_slice &tokens,                                     //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs            //
) {
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->token == TOK_IDENTIFIER) {
            const std::string it_lexme(it->lexme);
            if (Matcher::token_match((it + 1)->token, Matcher::assignment_shorthand_operator) && (it + 2) != tokens.second) {
                if (scope->variables.find(it_lexme) == scope->variables.end()) {
                    THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, it->line, it->column, it_lexme);
                    return std::nullopt;
                }
                if (!std::get<2>(scope->variables.at(it_lexme))) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, it->line, it->column, it_lexme);
                    return std::nullopt;
                }
                std::shared_ptr<Type> expected_type = std::get<0>(scope->variables.at(it_lexme));
                // Parse the expression with the expected type passed into it
                token_slice expression_tokens = {it + 2, tokens.second};
                std::optional<std::unique_ptr<ExpressionNode>> expression;
                if (rhs.has_value()) {
                    expression = std::move(rhs.value());
                } else {
                    expression = create_expression(scope, expression_tokens, expected_type);
                }
                if (!expression.has_value()) {
                    return std::nullopt;
                }
                Token op;
                switch (std::next(it)->token) {
                    default:
                        // It should never come here
                        assert(false);
                    case TOK_PLUS_EQUALS:
                        op = TOK_PLUS;
                        break;
                    case TOK_MINUS_EQUALS:
                        op = TOK_MINUS;
                        break;
                    case TOK_MULT_EQUALS:
                        op = TOK_MULT;
                        break;
                    case TOK_DIV_EQUALS:
                        op = TOK_DIV;
                        break;
                }
                std::unique_ptr<ExpressionNode> var_node = std::make_unique<VariableNode>(it_lexme, expected_type);
                std::unique_ptr<ExpressionNode> bin_op = std::make_unique<BinaryOpNode>( //
                    op, var_node, expression.value(), expected_type, true                //
                );
                return AssignmentNode(expected_type, it_lexme, bin_op, true);
            } else {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<GroupDeclarationNode> Parser::create_group_declaration( //
    std::shared_ptr<Scope> scope,                                     //
    const token_slice &tokens,                                        //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs               //
) {
    token_slice tokens_mut = tokens;
    std::optional<GroupDeclarationNode> declaration = std::nullopt;
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> variables;

    std::optional<uint2> lhs_range = Matcher::get_next_match_range(tokens_mut, Matcher::until_colon_equal);
    if (!lhs_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    token_slice lhs_tokens = {tokens_mut.first + lhs_range.value().first, tokens_mut.first + lhs_range.value().second};
    tokens_mut.first = lhs_tokens.second;

    // The last token now should be the COLON_EQUAL
    assert(std::prev(lhs_tokens.second)->token == TOK_COLON_EQUAL);
    lhs_tokens.second--;
    remove_surrounding_paren(lhs_tokens);
    while (lhs_tokens.first != lhs_tokens.second) {
        std::optional<uint2> var_range = Matcher::get_next_match_range(lhs_tokens, Matcher::until_comma);
        if (!var_range.has_value()) {
            // The whole lhs tokens is the last variable
            assert(std::prev(lhs_tokens.second)->token == TOK_IDENTIFIER);
            variables.emplace_back(nullptr, std::prev(lhs_tokens.second)->lexme);
            break;
        } else {
            token_slice var_tokens = {lhs_tokens.first + var_range.value().first, lhs_tokens.first + var_range.value().second};
            lhs_tokens.first = var_tokens.second;
            // The last token now should be the comma
            assert(std::prev(var_tokens.second)->token == TOK_COMMA);
            var_tokens.second--;
            // The last element is the variable name now
            assert(std::prev(var_tokens.second)->token == TOK_IDENTIFIER);
            variables.emplace_back(nullptr, std::prev(var_tokens.second)->lexme);
        }
    }
    // Now parse the expression (rhs)
    std::optional<std::unique_ptr<ExpressionNode>> expression = std::nullopt;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
    } else {
        expression = create_expression(scope, tokens_mut);
    }
    if (!expression.has_value()) {
        return std::nullopt;
    }
    const GroupType *group_type = dynamic_cast<const GroupType *>(expression.value()->type.get());
    if (group_type == nullptr) {
        // Rhs could be a multi-type
        if (const MultiType *multi_type = dynamic_cast<const MultiType *>(expression.value()->type.get())) {
            for (unsigned int i = 0; i < variables.size(); i++) {
                variables.at(i).first = multi_type->base_type;
                if (!scope->add_variable(variables.at(i).second, multi_type->base_type, scope->scope_id, true, false)) {
                    THROW_ERR( //
                        ErrVarRedefinition, ERR_PARSING, file_name, lhs_tokens.first->line, lhs_tokens.first->column,
                        variables.at(i).second //
                    );
                    return std::nullopt;
                }
            }
            std::string group_type_str = "(";
            for (unsigned int i = 0; i < multi_type->width; i++) {
                if (i > 0) {
                    group_type_str += ", ";
                }
                group_type_str += multi_type->base_type->to_string();
            }
            group_type_str += ")";
            std::optional<std::shared_ptr<Type>> expr_group_type = Type::get_type_from_str(group_type_str);
            if (!expr_group_type.has_value()) {
                std::vector<std::shared_ptr<Type>> group_types;
                for (unsigned int i = 0; i < multi_type->width; i++) {
                    group_types.emplace_back(multi_type->base_type);
                }
                expr_group_type = std::make_shared<GroupType>(group_types);
                Type::add_type(expr_group_type.value());
            }
            expression = std::make_unique<TypeCastNode>(expr_group_type.value(), expression.value());
            return GroupDeclarationNode(variables, expression.value());
        } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(expression.value()->type.get())) {
            if (variables.size() != tuple_type->types.size()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            for (unsigned int i = 0; i < variables.size(); i++) {
                variables.at(i).first = tuple_type->types[i];
                if (!scope->add_variable(variables.at(i).second, tuple_type->types[i], scope->scope_id, true, false)) {
                    THROW_ERR( //
                        ErrVarRedefinition, ERR_PARSING, file_name, lhs_tokens.first->line, lhs_tokens.first->column,
                        variables.at(i).second //
                    );
                    return std::nullopt;
                }
            }
            std::string group_type_str = "(";
            for (unsigned int i = 0; i < tuple_type->types.size(); i++) {
                if (i > 0) {
                    group_type_str += ", ";
                }
                group_type_str += tuple_type->types[i]->to_string();
            }
            group_type_str += ")";
            std::optional<std::shared_ptr<Type>> expr_group_type = Type::get_type_from_str(group_type_str);
            if (!expr_group_type.has_value()) {
                std::vector<std::shared_ptr<Type>> group_types;
                for (unsigned int i = 0; i < tuple_type->types.size(); i++) {
                    group_types.emplace_back(tuple_type->types[i]);
                }
                expr_group_type = std::make_shared<GroupType>(group_types);
                Type::add_type(expr_group_type.value());
            }
            expression = std::make_unique<TypeCastNode>(expr_group_type.value(), expression.value());
            return GroupDeclarationNode(variables, expression.value());
        } else {
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return std::nullopt;
        }
    }
    assert(group_type != nullptr);
    const std::vector<std::shared_ptr<Type>> &types = group_type->types;
    assert(variables.size() == types.size());
    for (unsigned int i = 0; i < variables.size(); i++) {
        variables.at(i).first = types.at(i);
        if (!scope->add_variable(variables.at(i).second, types.at(i), scope->scope_id, true, false)) {
            // Variable shadowing
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, lhs_tokens.first->line, lhs_tokens.first->column, variables.at(i).second);
            return std::nullopt;
        }
    }

    return GroupDeclarationNode(variables, expression.value());
}

std::optional<DeclarationNode> Parser::create_declaration( //
    std::shared_ptr<Scope> scope,                          //
    const token_slice &tokens,                             //
    const bool is_inferred,                                //
    const bool has_rhs,                                    //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs    //
) {
    token_slice tokens_mut = tokens;
    assert(!(is_inferred && !has_rhs));

    token_slice lhs_tokens;
    if (has_rhs) {
        uint2 lhs_range = Matcher::get_next_match_range(tokens_mut, Matcher::until_eq_or_colon_equal).value();
        lhs_tokens = {tokens_mut.first + lhs_range.first, tokens_mut.first + lhs_range.second};
        tokens_mut.first = lhs_tokens.second;
    } else {
        lhs_tokens = tokens_mut;
    }

    // Check if the first token of the declaration is a `const` or `mut` token and set the mutability accordingly
    const bool is_mutable = lhs_tokens.first->token != TOK_CONST;
    if (lhs_tokens.first->token == TOK_CONST || lhs_tokens.first->token == TOK_MUT) {
        lhs_tokens.first++;
    }

    // Get the type if it is not inferred and get the name of the declaration
    std::shared_ptr<Type> type;
    std::string name;
    if (!is_inferred) {
        assert(lhs_tokens.first->token == TOK_TYPE);
        type = lhs_tokens.first->type;
        assert(std::next(lhs_tokens.first)->token == TOK_IDENTIFIER);
        name = std::next(lhs_tokens.first)->lexme;
    } else {
        assert(lhs_tokens.first->token == TOK_IDENTIFIER);
        name = lhs_tokens.first->lexme;
    }

    // Add the variable to the variable list
    if (!has_rhs) {
        assert(!is_inferred);
        assert(type != nullptr);
        std::optional<std::unique_ptr<ExpressionNode>> expr = std::nullopt;
        if (!scope->add_variable(name, type, scope->scope_id, is_mutable, false)) {
            // Variable shadowing
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name,                            //
                std::next(lhs_tokens.first)->line, std::next(lhs_tokens.first)->column, name //
            );
            return std::nullopt;
        }
        return DeclarationNode(type, name, expr);
    }

    // If the type of the variable is inferred we need to create the expression with no expected type specified
    if (is_inferred) {
        std::optional<std::unique_ptr<ExpressionNode>> expr = std::nullopt;
        if (rhs.has_value()) {
            expr = std::move(rhs.value());
        } else {
            expr = create_expression(scope, tokens_mut);
        }
        if (!expr.has_value()) {
            return std::nullopt;
        }
        if (dynamic_cast<const GroupType *>(expr.value()->type.get())) {
            // The type of a group cannot be inferred
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (!scope->add_variable(name, expr.value()->type, scope->scope_id, is_mutable, false)) {
            // Variable shadowing
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, lhs_tokens.first->line, lhs_tokens.first->column, name);
            return std::nullopt;
        }
        return DeclarationNode(expr.value()->type, name, expr);
    }
    if (!scope->add_variable(name, type, scope->scope_id, is_mutable, false)) {
        // Variable shadowing
        THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name,                            //
            std::next(lhs_tokens.first)->line, std::next(lhs_tokens.first)->column, name //
        );
        return std::nullopt;
    }
    // When the type is known we pass it to the create_expression function so that it can check for the needed type
    if (rhs.has_value()) {
        return DeclarationNode(type, name, rhs);
    }
    auto expr = create_expression(scope, tokens_mut, type);
    if (!expr.has_value()) {
        return std::nullopt;
    }
    return DeclarationNode(type, name, expr);
}

std::optional<UnaryOpStatement> Parser::create_unary_op_statement(std::shared_ptr<Scope> scope, const token_slice &tokens) {
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

std::optional<DataFieldAssignmentNode> Parser::create_data_field_assignment( //
    std::shared_ptr<Scope> scope,                                            //
    const token_slice &tokens,                                               //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                      //
) {
    // Everything up to the equals sign is the lhs of the assignment
    token_slice tokens_mut = tokens;
    token_slice lhs_tokens = {tokens.first, tokens.first};
    while (lhs_tokens.second->token != TOK_EQUAL) {
        lhs_tokens.second++;
        tokens_mut.first++;
    }
    auto field_access_base = create_field_access_base(scope, lhs_tokens);
    if (!field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now the equal sign should follow, we will delete that one too
    assert(tokens_mut.first->token == TOK_EQUAL);
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
    } else {
        expression = create_expression(scope, tokens_mut);
    }
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // The data field base expression should be a variable expression
    std::unique_ptr<ExpressionNode> &base_expr = std::get<0>(field_access_base.value());
    const VariableNode *var_node = dynamic_cast<const VariableNode *>(base_expr.get());
    const std::string &var_name = var_node->name;
    if (var_node == nullptr) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    if (!std::get<2>(scope->variables.at(var_name))) {
        THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, tokens.first->line, tokens.first->column, var_name);
        return std::nullopt;
    }

    const auto &field_type = std::get<3>(field_access_base.value());
    if (field_type != expression.value()->type) {
        const auto castability = check_castability(field_type, expression.value()->type);
        if (!castability.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (!castability.value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        expression = std::make_unique<TypeCastNode>(field_type, expression.value());
    }

    // WARNING: There could be a possible memory corruption here, as the `base_expr` unique pointer goes out of scope here. This warning can
    // be deleted if it is safe, but it *could* be that this will cause problems down the line
    return DataFieldAssignmentNode(             //
        base_expr->type,                        // data_type
        var_name,                               // var_name
        std::get<1>(field_access_base.value()), // field_name
        std::get<2>(field_access_base.value()), // field_id
        field_type,                             // field_type
        expression.value()                      //
    );
}

std::optional<GroupedDataFieldAssignmentNode> Parser::create_grouped_data_field_assignment( //
    std::shared_ptr<Scope> scope,                                                           //
    const token_slice &tokens,                                                              //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                                     //
) {
    token_slice tokens_mut = tokens;
    token_slice lhs_tokens = {tokens.first, tokens.first};
    while (lhs_tokens.second->token != TOK_EQUAL) {
        lhs_tokens.second++;
        tokens_mut.first++;
    }
    auto grouped_field_access_base = create_grouped_access_base(scope, lhs_tokens);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const VariableNode *var_node = dynamic_cast<const VariableNode *>(std::get<0>(grouped_field_access_base.value()).get());
    if (var_node == nullptr) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now the equal sign should follow, we will delete that one too
    assert(tokens_mut.first->token == TOK_EQUAL);
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
    } else {
        expression = create_expression(scope, tokens_mut);
    }
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    if (!std::get<2>(scope->variables.at(var_node->name))) {
        THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, tokens.first->line, tokens.first->column, var_node->name);
        return std::nullopt;
    }

    return GroupedDataFieldAssignmentNode(                    //
        std::get<0>(grouped_field_access_base.value())->type, // data_type
        var_node->name,                                       // var_name
        std::get<1>(grouped_field_access_base.value()),       // field_names
        std::get<2>(grouped_field_access_base.value()),       // field_ids
        std::get<3>(grouped_field_access_base.value()),       // field_types
        expression.value()                                    //
    );
}

std::optional<GroupedDataFieldAssignmentNode> Parser::create_grouped_data_field_assignment_shorthand( //
    std::shared_ptr<Scope> scope,                                                                     //
    const token_slice &tokens,                                                                        //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                                               //
) {
    token_slice tokens_mut = tokens;
    token_slice lhs_tokens = {tokens.first, tokens.first};
    while (!Matcher::token_match(lhs_tokens.second->token, Matcher::assignment_shorthand_operator)) {
        lhs_tokens.second++;
        tokens_mut.first++;
    }
    auto grouped_field_access_base = create_grouped_access_base(scope, lhs_tokens);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const VariableNode *var_node = dynamic_cast<const VariableNode *>(std::get<0>(grouped_field_access_base.value()).get());
    if (var_node == nullptr) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Get the operation of the assignment shorthand
    Token operation = TOK_EOF;
    switch (tokens_mut.first->token) {
        case TOK_PLUS_EQUALS:
            operation = TOK_PLUS;
            break;
        case TOK_MINUS_EQUALS:
            operation = TOK_MINUS;
            break;
        case TOK_MULT_EQUALS:
            operation = TOK_MULT;
            break;
        case TOK_DIV_EQUALS:
            operation = TOK_DIV;
            break;
        default:
            assert(false);
            break;
    }
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
    } else {
        expression = create_expression(scope, tokens_mut);
    }
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Check if the data variable we try to mutate is marked as immutable
    if (!std::get<2>(scope->variables.at(var_node->name))) {
        THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, tokens.first->line, tokens.first->column, var_node->name);
        return std::nullopt;
    }

    // Create the lhs of the binary op of the right side, which is the grouped data field access
    const std::shared_ptr<Type> &base_expr_type = std::get<0>(grouped_field_access_base.value())->type;
    std::unique_ptr<ExpressionNode> binop_lhs = std::make_unique<GroupedDataAccessNode>( //
        std::get<0>(grouped_field_access_base.value()),                                  // base_expr
        std::get<1>(grouped_field_access_base.value()),                                  // field_names
        std::get<2>(grouped_field_access_base.value()),                                  // field_ids
        std::get<3>(grouped_field_access_base.value())                                   // field_types
    );

    // The expression already is the rhs of the binop, so now we only need to check whether the types of the expression matches the types of
    // the assignment
    expression = std::make_unique<BinaryOpNode>( //
        operation,                               //
        binop_lhs,                               //
        expression.value(),                      //
        binop_lhs->type,                         //
        true                                     //
    );

    return GroupedDataFieldAssignmentNode(              //
        base_expr_type,                                 // data_type
        var_node->name,                                 // var_name
        std::get<1>(grouped_field_access_base.value()), // field_names
        std::get<2>(grouped_field_access_base.value()), // field_ids
        std::get<3>(grouped_field_access_base.value()), // field_types
        expression.value()                              //
    );
}

std::optional<ArrayAssignmentNode> Parser::create_array_assignment( //
    std::shared_ptr<Scope> scope,                                   //
    const token_slice &tokens,                                      //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs             //
) {
    token_slice tokens_mut = tokens;
    // Now the first token should be the array identifier
    assert(tokens_mut.first->token == TOK_IDENTIFIER);
    const std::string variable_name(tokens_mut.first->lexme);
    std::optional<std::shared_ptr<Type>> var_type = scope->get_variable_type(variable_name);
    if (!var_type.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const ArrayType *array_type = dynamic_cast<const ArrayType *>(var_type.value().get());
    if (array_type == nullptr) {
        // Accessed variable not of array type
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    tokens_mut.first++;
    // The next token should be a [ symbol
    std::optional<uint2> bracket_range = Matcher::balanced_range_extraction(            //
        tokens_mut, Matcher::token(TOK_LEFT_BRACKET), Matcher::token(TOK_RIGHT_BRACKET) //
    );
    if (!bracket_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    token_slice indexing_tokens = {tokens_mut.first + bracket_range.value().first, tokens_mut.first + bracket_range.value().second};
    tokens_mut.first = indexing_tokens.second;
    // Now the first two tokens should be the [ and ]
    assert(indexing_tokens.first->token == TOK_LEFT_BRACKET);
    indexing_tokens.first++;
    assert(std::prev(indexing_tokens.second)->token == TOK_RIGHT_BRACKET);
    indexing_tokens.second--;

    auto indexing_expressions = create_group_expressions(scope, indexing_tokens);
    if (!indexing_expressions.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Now the next token should be a = sign
    assert(tokens_mut.first->token == TOK_EQUAL);
    tokens_mut.first++;

    if (rhs.has_value()) {
        return ArrayAssignmentNode(variable_name, var_type.value(), array_type->type, indexing_expressions.value(), rhs.value());
    }
    // Parse the rhs expression
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, tokens_mut, array_type->type);
    if (!expression.has_value()) {
        return std::nullopt;
    }
    return ArrayAssignmentNode(variable_name, var_type.value(), array_type->type, indexing_expressions.value(), expression.value());
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_stacked_statement(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    if (!Matcher::tokens_contain(tokens, Matcher::token(TOK_EQUAL))) {
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    // Stacked statements cant be declarations, because sub-elements of a "stack" like `var.field.field` are already declared when one
    // can write a statement like this. This means the stacked statement is definitely an assignment. First, find the position of the
    // equals sign
    auto iterator = tokens.first;
    while (iterator != tokens.second && iterator->token != TOK_EQUAL) {
        ++iterator;
    }
    assert(iterator->token == TOK_EQUAL);
    // Now, everything to the right of the iterator is the rhs expression
    const token_slice rhs_expr_tokens = {iterator + 1, tokens.second};
    auto rhs_expr = create_expression(scope, rhs_expr_tokens);
    if (!rhs_expr.has_value()) {
        return std::nullopt;
    }
    // Okay, so now we find the dot token for the last stack and create an expression with everything to the left of it
    --iterator;
    size_t depth = 0;
    for (; iterator != tokens.first; --iterator) {
        if (iterator->token == TOK_RIGHT_PAREN) {
            depth++;
        } else if (iterator->token == TOK_LEFT_PAREN) {
            depth--;
        } else if (iterator->token == TOK_DOT && depth == 0) {
            break;
        }
    }
    // Okay, now everything to the left of the iterator is the base expression
    const token_slice base_expr_tokens = {tokens.first, iterator};
    auto base_expr = create_expression(scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        return std::nullopt;
    }
    ++iterator;
    const std::shared_ptr<Type> base_expr_type = base_expr.value()->type;
    if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(base_expr_type.get())) {
        if (iterator->token != TOK_DOLLAR || std::next(iterator)->token != TOK_INT_VALUE) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // Handle the special case of tuple / multi-type accesses in a stacked expression
        size_t field_id = std::stoul(std::string(std::next(iterator)->lexme));
        const std::string field_name = "$" + std::to_string(field_id);
        if (field_id >= tuple_type->types.size()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<StackedAssignmentNode>(                                               //
            base_expr.value(), field_name, field_id, tuple_type->types.at(field_id), rhs_expr.value() //
        );
    } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(base_expr_type.get())) {
        if (iterator->token == TOK_IDENTIFIER) {
            if (multi_type->width > 4) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            size_t field_id = 0;
            const std::string field_name(iterator->lexme);
            if (multi_type->width == 4) {
                if (field_name == "r") {
                    field_id = 0;
                } else if (field_name == "g") {
                    field_id = 1;
                } else if (field_name == "b") {
                    field_id = 2;
                } else if (field_name == "w") {
                    field_id = 3;
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            } else {
                if (field_name == "x") {
                    field_id = 0;
                } else if (field_name == "y") {
                    field_id = 1;
                } else if (field_name == "z") {
                    field_id = 2;
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            }
            return std::make_unique<StackedAssignmentNode>(                                      //
                base_expr.value(), field_name, field_id, multi_type->base_type, rhs_expr.value() //
            );
        } else if (iterator->token == TOK_DOLLAR && std::next(iterator)->token == TOK_INT_VALUE) {
            // Handle the special case of tuple / multi-type accesses in a stacked expression
            size_t field_id = std::stoul(std::string(std::next(iterator)->lexme));
            const std::string field_name = "$" + std::to_string(field_id);
            if (field_id >= multi_type->width) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<StackedAssignmentNode>(                                      //
                base_expr.value(), field_name, field_id, multi_type->base_type, rhs_expr.value() //
            );
        } else {
            assert(false);
        }
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(base_expr_type.get())) {
        if (iterator->token == TOK_IDENTIFIER) {
            // It's a single value assignment
            const std::string field_name(iterator->lexme);
            const DataNode *data_node = data_type->data_node;
            auto field_it = data_node->fields.begin();
            while (field_it != data_node->fields.end()) {
                if (std::get<0>(*field_it) == field_name) {
                    break;
                }
                ++field_it;
            }
            if (field_it == data_node->fields.end()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            size_t field_id = std::distance(data_node->fields.begin(), field_it);
            const std::shared_ptr<Type> field_type = std::get<1>(*field_it);
            return std::make_unique<StackedAssignmentNode>(base_expr.value(), field_name, field_id, field_type, rhs_expr.value());
        } else if (iterator->token == TOK_LEFT_PAREN) {
            // It's a grouped assignment
            std::vector<std::string> field_names;
            std::vector<unsigned int> field_ids;
            std::vector<std::shared_ptr<Type>> field_types;
            const DataNode *data_node = data_type->data_node;
            ++iterator;
            while (iterator != tokens.second && iterator->token != TOK_RIGHT_PAREN) {
                if (iterator->token == TOK_IDENTIFIER) {
                    const std::string field_str(iterator->lexme);
                    bool field_found = false;
                    for (auto field_it = data_node->fields.begin(); field_it != data_node->fields.end(); ++field_it) {
                        if (field_it->first == field_str) {
                            field_ids.emplace_back(std::distance(data_node->fields.begin(), field_it));
                            field_names.emplace_back(field_it->first);
                            field_types.emplace_back(field_it->second);
                            field_found = true;
                            break;
                        }
                    }
                    if (!field_found) {
                        // Use of non-present field in data type
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                } else if (iterator->token != TOK_COMMA) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                ++iterator;
            }
            if (field_names.empty()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<StackedGroupedAssignmentNode>(base_expr.value(), field_names, field_ids, field_types, rhs_expr.value());
        } else {
            assert(false);
        }
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return std::nullopt;
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_statement( //
    std::shared_ptr<Scope> scope,                                       //
    const token_slice &tokens,                                          //
    std::optional<std::unique_ptr<ExpressionNode>> rhs                  //
) {
    std::optional<std::unique_ptr<StatementNode>> statement_node = std::nullopt;

    if (Matcher::tokens_contain(tokens, Matcher::group_declaration_inferred)) {
        std::optional<GroupDeclarationNode> group_decl = create_group_declaration(scope, tokens, rhs);
        if (!group_decl.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupDeclarationNode>(std::move(group_decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::declaration_explicit)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, tokens, false, true, rhs);
        if (!decl.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::declaration_inferred)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, tokens, true, true, rhs);
        if (!decl.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::declaration_without_initializer)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, tokens, false, false, rhs);
        if (!decl.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::data_field_assignment)) {
        std::optional<DataFieldAssignmentNode> assign = create_data_field_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<DataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::grouped_data_assignment)) {
        std::optional<GroupedDataFieldAssignmentNode> assign = create_grouped_data_field_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupedDataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::grouped_data_assignment_shorthand)) {
        std::optional<GroupedDataFieldAssignmentNode> assign = create_grouped_data_field_assignment_shorthand(scope, tokens, rhs);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupedDataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::group_assignment)) {
        std::optional<GroupAssignmentNode> assign = create_group_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::group_assignment_shorthand)) {
        std::optional<GroupAssignmentNode> assign = create_group_assignment_shorthand(scope, tokens, rhs);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::array_assignment)) {
        std::optional<ArrayAssignmentNode> assign = create_array_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<ArrayAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::assignment)) {
        std::optional<AssignmentNode> assign = create_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<AssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::assignment_shorthand)) {
        std::optional<AssignmentNode> assign = create_assignment_shorthand(scope, tokens, rhs);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<AssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::return_statement)) {
        std::optional<ReturnNode> return_node = create_return(scope, tokens);
        if (!return_node.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<ReturnNode>(std::move(return_node.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::throw_statement)) {
        std::optional<ThrowNode> throw_node = create_throw(scope, tokens);
        if (!throw_node.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<ThrowNode>(std::move(throw_node.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::aliased_function_call)) {
        token_slice tokens_mut = tokens;
        assert(tokens_mut.first->token == TOK_IDENTIFIER && std::next(tokens_mut.first)->token == TOK_DOT);
        const std::string alias_base(tokens_mut.first->lexme);
        if (aliases.find(alias_base) == aliases.end()) {
            THROW_ERR(ErrAliasNotFound, ERR_PARSING, file_name, tokens.first->line, tokens.first->column, alias_base);
            return std::nullopt;
        }
        tokens_mut.first += 2;
        statement_node = create_call_statement(scope, tokens_mut, alias_base);
    } else if (Matcher::tokens_contain(tokens, Matcher::function_call)) {
        statement_node = create_call_statement(scope, tokens, std::nullopt);
    } else if (Matcher::tokens_contain(tokens, Matcher::unary_op_expr)) {
        std::optional<UnaryOpStatement> unary_op = create_unary_op_statement(scope, tokens);
        if (!unary_op.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<UnaryOpStatement>(std::move(unary_op.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::break_statement)) {
        statement_node = std::make_unique<BreakNode>();
    } else if (Matcher::tokens_contain(tokens, Matcher::continue_statement)) {
        statement_node = std::make_unique<ContinueNode>();
    } else {
        token_list toks = clone_from_slice(tokens);
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return statement_node;
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_scoped_statement( //
    std::shared_ptr<Scope> scope,                                              //
    std::vector<Line>::const_iterator &line_it,                                //
    const std::vector<Line> &body,                                             //
    std::vector<std::unique_ptr<StatementNode>> &statements                    //
) {
    std::optional<std::unique_ptr<StatementNode>> statement_node = std::nullopt;
    auto definition_it = line_it;

    auto get_scoped_body = [this, &body](std::vector<Line>::const_iterator &line_it) -> std::optional<std::vector<Line>> {
        const unsigned int indent_lvl_statement = line_it->indent_lvl;
        auto scoped_body_begin = ++line_it;
        unsigned int indent_lvl_line = line_it->indent_lvl;
        while (line_it != body.end() && indent_lvl_line > indent_lvl_statement) {
            ++line_it;
            indent_lvl_line = line_it->indent_lvl;
        }
        if (line_it == scoped_body_begin) {
            // No body found after scoped statement definition line, so we go back to the definition's line
            THROW_ERR(ErrMissingBody, ERR_PARSING, file_name, (line_it - 1)->tokens);
            return std::nullopt;
        }
        std::vector<Line> scoped_body(scoped_body_begin, line_it);
        return scoped_body;
    };
    std::optional<std::vector<Line>> scoped_body = get_scoped_body(line_it);
    if (!scoped_body.has_value()) {
        return std::nullopt;
    }

    const token_slice definition = {definition_it->tokens.first, definition_it->tokens.second};
    if (Matcher::tokens_contain(definition, Matcher::if_statement)         //
        || Matcher::tokens_contain(definition, Matcher::else_if_statement) //
        || Matcher::tokens_contain(definition, Matcher::else_statement)    //
    ) {
        if (Matcher::tokens_contain(definition, Matcher::token(TOK_ELSE))) {
            // else or else if at top of if chain
            THROW_ERR(ErrStmtIfChainMissingIf, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        std::vector<std::pair<token_slice, std::vector<Line>>> if_chain;
        if_chain.emplace_back(definition, std::move(scoped_body.value()));

        token_slice next_definition = definition;
        while (true) {
            if (line_it == body.end()) {
                break;
            }
            // Check if the line after the if body contains an else token, only if it contains an else statemnt the chain continues
            next_definition = {line_it->tokens.first, line_it->tokens.second};
            if (!Matcher::tokens_contain(next_definition, Matcher::token(TOK_ELSE))) {
                break;
            }
            scoped_body = get_scoped_body(line_it);
            if_chain.emplace_back(next_definition, std::move(scoped_body.value()));
        }

        std::optional<std::unique_ptr<IfNode>> if_node = create_if(scope, if_chain);
        if (!if_node.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(if_node.value());
    } else if (Matcher::tokens_contain(definition, Matcher::for_loop)) {
        std::optional<std::unique_ptr<ForLoopNode>> for_loop = create_for_loop(scope, definition, scoped_body.value());
        if (!for_loop.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(for_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::par_for_loop) ||
        Matcher::tokens_contain(definition, Matcher::enhanced_for_loop)) {
        std::optional<std::unique_ptr<EnhForLoopNode>> enh_for_loop = create_enh_for_loop(scope, definition, scoped_body.value());
        if (!enh_for_loop.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(enh_for_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::while_loop)) {
        std::optional<std::unique_ptr<WhileNode>> while_loop = create_while_loop(scope, definition, scoped_body.value());
        if (!while_loop.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(while_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::catch_statement) //
        || Matcher::tokens_contain(definition, Matcher::token(TOK_CATCH))    //
    ) {
        std::optional<std::unique_ptr<CatchNode>> catch_node = create_catch(scope, definition, scoped_body.value(), statements);
        if (!catch_node.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(catch_node.value());
    } else if (Matcher::tokens_contain(definition, Matcher::aliased_function_call)) {
        assert(definition.first->token == TOK_IDENTIFIER && std::next(definition.first)->token == TOK_DOT);
        const std::string alias_base(definition.first->lexme);
        statement_node = create_call_statement(scope, {definition.first + 2, definition.second}, alias_base);
    } else if (Matcher::tokens_contain(definition, Matcher::function_call)) {
        statement_node = create_call_statement(scope, definition, std::nullopt);
    } else if (Matcher::tokens_contain(definition, Matcher::switch_statement)) {
        statement_node = create_switch_statement(scope, definition, scoped_body.value());
    } else {
        // Unknown scoped statement
        token_list toks = clone_from_slice(definition);
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return statement_node;
}

std::optional<std::vector<std::unique_ptr<StatementNode>>> Parser::create_body(std::shared_ptr<Scope> scope,
    const std::vector<Line> &body) {
    std::vector<std::unique_ptr<StatementNode>> body_statements;

    for (auto line_it = body.begin(); line_it != body.end();) {
        token_slice statement_tokens = {line_it->tokens.first, line_it->tokens.second};
        std::optional<std::unique_ptr<StatementNode>> next_statement = std::nullopt;
        const bool is_scoped = std::prev(statement_tokens.second)->token == TOK_COLON;
        if (is_scoped) {
            // --- SCOPED STATEMENT (IF, LOOPS, CATCH-BLOCK, SWITCH) ---
            next_statement = create_scoped_statement(scope, line_it, body, body_statements);
        } else {
            if (Matcher::tokens_start_with(statement_tokens, Matcher::stacked_expression)) {
                // --- STACKED STATEMENT ---
                next_statement = create_stacked_statement(scope, statement_tokens);
            } else {
                // --- NORMAL STATEMENT ---
                next_statement = create_statement(scope, statement_tokens);
            }
        }
        if (!next_statement.has_value()) {
            return std::nullopt;
        }
        body_statements.emplace_back(std::move(next_statement.value()));

        // Only increment the line iterator if it was no scoped statement, as the create_scoped_statement function already moves the line
        // iterator to the line *after* it's scoped block
        if (!is_scoped) {
            line_it++;
        }
    }

    return body_statements;
}
