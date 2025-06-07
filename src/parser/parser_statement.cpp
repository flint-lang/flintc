#include "parser/parser.hpp"

#include "error/error.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/statements/stacked_assignment.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "types.hpp"

#include <iterator>
#include <optional>
#include <string>
#include <variant>

std::optional<std::unique_ptr<CallNodeStatement>> Parser::create_call_statement( //
    Scope *scope,                                                                //
    const token_slice &tokens,                                                   //
    const std::optional<std::string> &alias_base                                 //
) {
    token_slice tokens_mut = tokens;
    auto call_node_args = create_call_or_initializer_base(scope, tokens_mut, alias_base);
    if (!call_node_args.has_value()) {
        THROW_ERR(ErrExprCallCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }
    if (std::get<3>(call_node_args.value()).has_value()) {
        // Initializers are not allowed to be statements
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
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

std::optional<ThrowNode> Parser::create_throw(Scope *scope, const token_slice &tokens) {
    unsigned int throw_id = 0;
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->type == TOK_THROW) {
            if (std::next(it) == tokens.second) {
                THROW_ERR(ErrStmtThrowCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            throw_id = std::distance(tokens.first, it);
        }
    }
    token_slice expression_tokens = {tokens.first + throw_id + 1, tokens.second};
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, expression_tokens, Type::get_primitive_type("i32"));
    if (!expr.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expression_tokens);
        return std::nullopt;
    }
    if (expr.value()->type->to_string() != "i32") {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, expression_tokens, Type::get_primitive_type("i32"), expr.value()->type);
        return std::nullopt;
    }
    return ThrowNode(expr.value());
}

std::optional<ReturnNode> Parser::create_return(Scope *scope, const token_slice &tokens) {
    unsigned int return_id = 0;
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->type == TOK_RETURN) {
            if (std::next(it) == tokens.second) {
                THROW_ERR(ErrStmtReturnCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            return_id = std::distance(tokens.first, it);
        }
    }
    token_slice expression_tokens = {tokens.first + return_id + 1, tokens.second};
    std::optional<std::unique_ptr<ExpressionNode>> return_expr;
    if (std::next(expression_tokens.first) == expression_tokens.second) {
        return ReturnNode(return_expr);
    }
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, expression_tokens);
    if (!expr.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expression_tokens);
        return std::nullopt;
    }
    return_expr = std::move(expr.value());
    return ReturnNode(return_expr);
}

std::optional<std::unique_ptr<IfNode>> Parser::create_if(Scope *scope, std::vector<std::pair<token_slice, token_slice>> &if_chain) {
    assert(!if_chain.empty());
    std::pair<token_slice, token_slice> this_if_pair = if_chain.at(0);
    if_chain.erase(if_chain.begin());

    bool has_if = false;
    bool has_else = false;
    // Remove everything in front of the expression (\n, \t, else, if)
    for (auto it = this_if_pair.first.first; it != this_if_pair.first.second; ++it) {
        if (it->type == TOK_ELSE) {
            has_else = true;
        } else if (it->type == TOK_IF) {
            has_if = true;
            this_if_pair.first.first++;
            break;
        }
        this_if_pair.first.first++;
    }
    // Remove everything after the expression (:, \n)
    for (auto rev_it = std::prev(this_if_pair.first.second); rev_it != this_if_pair.first.first; --rev_it) {
        if (rev_it->type == TOK_COLON) {
            this_if_pair.first.second--;
            break;
        }
        this_if_pair.first.second--;
    }

    // Dangling else statement without if statement
    if (has_else && !has_if) {
        THROW_ERR(ErrStmtDanglingElse, ERR_PARSING, file_name, this_if_pair.first);
        return std::nullopt;
    }

    // Create the if statements condition and body statements
    std::optional<std::unique_ptr<ExpressionNode>> condition =
        create_expression(scope, this_if_pair.first, Type::get_primitive_type("bool"));
    if (!condition.has_value()) {
        // Invalid expression inside if statement
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, this_if_pair.first);
        return std::nullopt;
    }
    if (condition.value()->type->to_string() != "bool") {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, this_if_pair.first, Type::get_primitive_type("bool"),
            condition.value()->type);
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
        if (Matcher::tokens_contain(if_chain.at(0).first, Matcher::token(TOK_IF))) {
            // 'else if'
            else_scope = create_if(scope, if_chain);
        } else {
            // 'else'
            std::unique_ptr<Scope> else_scope_ptr = std::make_unique<Scope>(scope);
            auto else_body_statements = create_body(else_scope_ptr.get(), if_chain.at(0).second);
            if (!else_body_statements.has_value()) {
                THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, file_name, if_chain.at(0).second);
                return std::nullopt;
            }
            else_scope_ptr->body = std::move(else_body_statements.value());
            else_scope = std::move(else_scope_ptr);
        }
    }

    return std::make_unique<IfNode>(condition.value(), body_scope, else_scope);
}

std::optional<std::unique_ptr<WhileNode>> Parser::create_while_loop( //
    Scope *scope,                                                    //
    const token_slice &definition,                                   //
    const token_slice &body                                          //
) {
    token_slice condition_tokens = definition;
    // Remove everything in front of the expression (\n, \t, else, if)
    for (auto it = condition_tokens.first; it != condition_tokens.second; ++it) {
        if (it->type == TOK_WHILE) {
            condition_tokens.first++;
            break;
        }
        condition_tokens.first++;
    }
    // Remove everything after the expression (:, \n)
    for (auto rev_it = std::prev(condition_tokens.second); rev_it != condition_tokens.first; --rev_it) {
        if (rev_it->type == TOK_COLON) {
            condition_tokens.second--;
            break;
        }
        condition_tokens.second--;
    }

    std::optional<std::unique_ptr<ExpressionNode>> condition = create_expression(scope, condition_tokens, Type::get_primitive_type("bool"));
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
    const token_slice &definition,                                   //
    const token_slice &body                                          //
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
    std::unique_ptr<Scope> definition_scope = std::make_unique<Scope>(scope);
    uint2 &initializer_range = expression_ranges.at(0);
    // "remove" everything including the 'for' keyword from the initializer statement
    // otherwise the for loop would create itself recursively
    for (unsigned int i = initializer_range.first; i < initializer_range.second; i++) {
        if ((definition.first + i)->type == TOK_FOR) {
            initializer_range.first = i + 1;
            break;
        }
    }
    // Parse the initializer statement
    token_slice initializer_tokens = {definition.first + initializer_range.first, definition.first + initializer_range.second};
    initializer = create_statement(definition_scope.get(), initializer_tokens);
    if (!initializer.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    definition_scope->body.emplace_back(std::move(initializer.value()));

    // Parse the loop condition expression
    uint2 &condition_range = expression_ranges.at(1);
    const token_slice condition_tokens = {definition.first + condition_range.first, definition.first + condition_range.second};
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
    token_slice looparound_tokens = {definition.first + condition_range.second, definition.first + expressions_range.value().second};
    looparound = create_statement(body_scope.get(), looparound_tokens);
    if (!looparound.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    body_scope->body.emplace_back(std::move(looparound.value()));

    return std::make_unique<ForLoopNode>(condition.value(), definition_scope, body_scope);
}

std::optional<std::unique_ptr<EnhForLoopNode>> Parser::create_enh_for_loop( //
    [[maybe_unused]] Scope *scope,                                          //
    const token_slice &definition,                                          //
    const token_slice &body                                                 //
) {
    token_slice definition_mut = definition;
    remove_leading_garbage(definition_mut);
    remove_trailing_garbage(definition_mut);

    // Now the first token should be the `for` token
    assert(definition_mut.first->type == TOK_FOR);
    definition_mut.first++;

    // The next token should either be a `(` or an identifer. If its an identifier we use the "tupled" enhanced for loop approach
    std::variant<std::pair<std::optional<std::string>, std::optional<std::string>>, std::string> iterators;
    if (definition_mut.first->type == TOK_IDENTIFIER) {
        // Its a tuple, e.g. `for t in iterable:` where `t` is of type `data<u64, T>`
        iterators = definition_mut.first->lexme;
        definition_mut.first++;
    } else {
        // Its a group, e.g. `for (index, element) in iterable:`
        assert(definition_mut.first->type == TOK_LEFT_PAREN);
        definition_mut.first++;
        std::optional<std::string> index_identifier;
        if (definition_mut.first->type == TOK_IDENTIFIER) {
            index_identifier = definition_mut.first->lexme;
        }
        definition_mut.first++;
        assert(definition_mut.first->type == TOK_COMMA);
        definition_mut.first++;
        std::optional<std::string> element_identifier;
        if (definition_mut.first->type == TOK_IDENTIFIER) {
            element_identifier = definition_mut.first->lexme;
        }
        definition_mut.first++;
        assert(definition_mut.first->type == TOK_RIGHT_PAREN);
        definition_mut.first++;
        iterators = std::make_pair(index_identifier, element_identifier);
    }
    assert(definition_mut.first->type == TOK_IN);
    definition_mut.first++;

    // Create the definition scope
    std::unique_ptr<Scope> definition_scope = std::make_unique<Scope>(scope);

    // The rest of the definition is the iterable expression
    std::optional<std::unique_ptr<ExpressionNode>> iterable = create_expression(definition_scope.get(), definition_mut);
    if (!iterable.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, definition_mut);
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
        if (!definition_scope->add_variable(tuple_name, tuple_type, definition_scope->scope_id, false, true)) {
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
            if (!definition_scope->add_variable(index_name.value(), index_type, definition_scope->scope_id, false, true)) {
                THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, index_it->line, index_it->column, index_name.value());
                return std::nullopt;
            }
        }
        if (element_name.has_value()) {
            auto element_it = definition_mut.first - 3;
            if (!definition_scope->add_variable(element_name.value(), element_type, definition_scope->scope_id, false, true)) {
                THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, element_it->line, element_it->column, element_name.value());
                return std::nullopt;
            }
        }
    }

    // Now create the body scope and parse the body
    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>(definition_scope.get());
    auto body_statements = create_body(body_scope.get(), body);
    if (!body_statements.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());

    return std::make_unique<EnhForLoopNode>(iterators, iterable.value(), definition_scope, body_scope);
}

std::optional<std::unique_ptr<CatchNode>> Parser::create_catch( //
    Scope *scope,                                               //
    const token_slice &definition,                              //
    const token_slice &body,                                    //
    std::vector<std::unique_ptr<StatementNode>> &statements     //
) {
    // First, extract everything left of the 'catch' statement and parse it as a normal (unscoped) statement
    std::optional<token_list::iterator> catch_id = std::nullopt;
    for (auto it = definition.first; it != definition.second; ++it) {
        if (it->type == TOK_CATCH) {
            catch_id = it;
            break;
        }
    }
    if (!catch_id.has_value()) {
        THROW_ERR(ErrStmtDanglingCatch, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }

    token_slice left_of_catch = {definition.first, catch_id.value()};
    std::optional<std::unique_ptr<StatementNode>> lhs = create_statement(scope, left_of_catch);
    if (!lhs.has_value()) {
        THROW_ERR(ErrStmtCreationFailed, ERR_PARSING, file_name, left_of_catch);
        return std::nullopt;
    }
    statements.emplace_back(std::move(lhs.value()));
    // Get the last parsed call and set the 'has_catch' property of the call node
    if (!last_parsed_call.has_value()) {
        THROW_ERR(ErrStmtDanglingCatch, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }
    last_parsed_call.value()->has_catch = true;

    const token_slice right_of_catch = {catch_id.value(), std::prev(definition.second)};
    std::optional<std::string> err_var = std::nullopt;
    for (auto it = right_of_catch.first; it != right_of_catch.second; ++it) {
        if (it->type == TOK_CATCH && std::next(it) != right_of_catch.second && std::next(it)->type == TOK_IDENTIFIER) {
            err_var = std::next(it)->lexme;
        }
    }

    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>(scope);
    if (err_var.has_value()) {
        if (!body_scope->add_variable(err_var.value(), Type::get_primitive_type("i32"), body_scope->scope_id, false, false)) {
            THROW_ERR(                                                                                                                //
                ErrVarRedefinition, ERR_PARSING, file_name, right_of_catch.first->line, right_of_catch.first->column, err_var.value() //
            );
        }
    }
    auto body_statements = create_body(body_scope.get(), body);
    if (!body_statements.has_value()) {
        THROW_ERR(ErrBodyCreationFailed, ERR_PARSING, file_name, body);
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());

    return std::make_unique<CatchNode>(err_var, body_scope, last_parsed_call.value());
}

std::optional<GroupAssignmentNode> Parser::create_group_assignment(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    remove_leading_garbage(tokens_mut);
    assert(tokens_mut.first != tokens_mut.second);
    // Now a left paren is expected as the start of the group assignment
    if (tokens_mut.first->type != TOK_LEFT_PAREN) {
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
        if (std::next(it) == tokens_mut.second || (std::next(it)->type != TOK_COMMA && std::next(it)->type != TOK_RIGHT_PAREN)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // This element is the assignee
        if (scope->variables.find(it->lexme) == scope->variables.end()) {
            THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, it->line, it->column, it->lexme);
            return std::nullopt;
        }
        if (!std::get<2>(scope->variables.at(it->lexme))) {
            THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, it->line, it->column, it->lexme);
            return std::nullopt;
        }
        const std::shared_ptr<Type> &expected_type = std::get<0>(scope->variables.at(it->lexme));
        assignees.emplace_back(expected_type, it->lexme);
        index += 2;
        if (std::next(it)->type == TOK_RIGHT_PAREN) {
            break;
        }
    }
    // Erase all the assignment tokens
    tokens_mut.first += index;
    // Now the first token should be an equals token
    if (tokens_mut.first->type != TOK_EQUAL) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Remove the equal sign
    tokens_mut.first++;
    // The rest of the tokens now is the expression
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, tokens_mut);
    if (!expr.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }
    return GroupAssignmentNode(assignees, expr.value());
}

std::optional<GroupAssignmentNode> Parser::create_group_assignment_shorthand(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    remove_leading_garbage(tokens_mut);
    assert(tokens_mut.first != tokens_mut.second);
    // Now a left paren is expected as the start of the group assignment
    if (tokens_mut.first->type != TOK_LEFT_PAREN) {
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
        if (std::next(it) == tokens_mut.second || (std::next(it)->type != TOK_COMMA && std::next(it)->type != TOK_RIGHT_PAREN)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // This element is the assignee
        if (scope->variables.find(it->lexme) == scope->variables.end()) {
            THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, it->line, it->column, it->lexme);
            return std::nullopt;
        }
        if (!std::get<2>(scope->variables.at(it->lexme))) {
            THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, it->line, it->column, it->lexme);
            return std::nullopt;
        }
        const std::shared_ptr<Type> &expected_type = std::get<0>(scope->variables.at(it->lexme));
        assignees.emplace_back(expected_type, it->lexme);
        index += 2;
        if (std::next(it)->type == TOK_RIGHT_PAREN) {
            break;
        }
    }
    // Erase all the assignment tokens
    tokens_mut.first += index;

    // Get the operation of the assignment shorthand
    Token operation = TOK_EOF;
    switch (tokens_mut.first->type) {
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
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, tokens_mut);
    if (!expr.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }

    // The expression now is the group of all assignees within a binop with the rhs expr
    std::vector<std::unique_ptr<ExpressionNode>> lhs_expressions;
    for (const auto &[type, name] : assignees) {
        lhs_expressions.emplace_back(std::make_unique<VariableNode>(name, type));
    }
    std::unique_ptr<ExpressionNode> lhs_expr = std::make_unique<GroupExpressionNode>(lhs_expressions);

    // The "real" expression of the assignment is a binop between the lhs and the "real" expression
    expr.value() = std::make_unique<BinaryOpNode>( //
        operation,                                 //
        lhs_expr,                                  //
        expr.value(),                              //
        lhs_expr->type,                            //
        true                                       //
    );

    return GroupAssignmentNode(assignees, expr.value());
}

std::optional<AssignmentNode> Parser::create_assignment(Scope *scope, const token_slice &tokens) {
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->type == TOK_IDENTIFIER) {
            if (std::next(it)->type == TOK_EQUAL && (it + 2) != tokens.second) {
                if (scope->variables.find(it->lexme) == scope->variables.end()) {
                    THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, it->line, it->column, it->lexme);
                    return std::nullopt;
                }
                if (!std::get<2>(scope->variables.at(it->lexme))) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, it->line, it->column, it->lexme);
                    return std::nullopt;
                }
                std::shared_ptr<Type> expected_type = std::get<0>(scope->variables.at(it->lexme));
                // Parse the expression with the expected type passed into it
                token_slice expression_tokens = {it + 2, tokens.second};
                std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, expression_tokens, expected_type);
                if (!expression.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expression_tokens);
                    return std::nullopt;
                }
                return AssignmentNode(expected_type, it->lexme, expression.value());
            } else {
                THROW_ERR(ErrStmtAssignmentCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<AssignmentNode> Parser::create_assignment_shorthand(Scope *scope, const token_slice &tokens) {
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->type == TOK_IDENTIFIER) {
            if (Matcher::tokens_match({it + 1, it + 2}, Matcher::assignment_shorthand_operator) && (it + 2) != tokens.second) {
                if (scope->variables.find(it->lexme) == scope->variables.end()) {
                    THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, it->line, it->column, it->lexme);
                    return std::nullopt;
                }
                if (!std::get<2>(scope->variables.at(it->lexme))) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, it->line, it->column, it->lexme);
                    return std::nullopt;
                }
                std::shared_ptr<Type> expected_type = std::get<0>(scope->variables.at(it->lexme));
                // Parse the expression with the expected type passed into it
                token_slice expression_tokens = {it + 2, tokens.second};
                std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, expression_tokens, expected_type);
                if (!expression.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expression_tokens);
                    return std::nullopt;
                }
                Token op;
                switch (std::next(it)->type) {
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
                std::unique_ptr<ExpressionNode> var_node = std::make_unique<VariableNode>(it->lexme, expected_type);
                std::unique_ptr<ExpressionNode> bin_op = std::make_unique<BinaryOpNode>( //
                    op, var_node, expression.value(), expected_type, true                //
                );
                return AssignmentNode(expected_type, it->lexme, bin_op, true);
            } else {
                THROW_ERR(ErrStmtAssignmentCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<GroupDeclarationNode> Parser::create_group_declaration(Scope *scope, const token_slice &tokens) {
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

    remove_leading_garbage(lhs_tokens);
    // The last token now should be the COLON_EQUAL
    assert(std::prev(lhs_tokens.second)->type == TOK_COLON_EQUAL);
    lhs_tokens.second--;
    remove_surrounding_paren(lhs_tokens);
    while (lhs_tokens.first != lhs_tokens.second) {
        std::optional<uint2> var_range = Matcher::get_next_match_range(lhs_tokens, Matcher::until_comma);
        if (!var_range.has_value()) {
            // The whole lhs tokens is the last variable
            assert(std::prev(lhs_tokens.second)->type == TOK_IDENTIFIER);
            variables.emplace_back(nullptr, std::prev(lhs_tokens.second)->lexme);
            break;
        } else {
            token_slice var_tokens = {lhs_tokens.first + var_range.value().first, lhs_tokens.first + var_range.value().second};
            lhs_tokens.first = var_tokens.second;
            // The last token now should be the comma
            assert(std::prev(var_tokens.second)->type == TOK_COMMA);
            var_tokens.second--;
            // The last element is the variable name now
            assert(std::prev(var_tokens.second)->type == TOK_IDENTIFIER);
            variables.emplace_back(nullptr, std::prev(var_tokens.second)->lexme);
        }
    }
    // Now parse the expression (rhs)
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, tokens_mut);
    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }
    const GroupType *group_type = dynamic_cast<const GroupType *>(expression.value()->type.get());
    if (group_type == nullptr) {
        // Rhs could be a multi-type
        const MultiType *multi_type = dynamic_cast<const MultiType *>(expression.value()->type.get());
        if (multi_type == nullptr) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        for (unsigned int i = 0; i < variables.size(); i++) {
            variables.at(i).first = multi_type->base_type;
            if (!scope->add_variable(variables.at(i).second, multi_type->base_type, scope->scope_id, true, false)) {
                THROW_ERR(                                                                                                               //
                    ErrVarRedefinition, ERR_PARSING, file_name, lhs_tokens.first->line, lhs_tokens.first->column, variables.at(i).second //
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
            expr_group_type.value() = std::make_shared<GroupType>(group_types);
            Type::add_type(expr_group_type.value());
        }
        expression.value() = std::make_unique<TypeCastNode>(expr_group_type.value(), expression.value());
        return GroupDeclarationNode(variables, expression.value());
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
    Scope *scope,                                          //
    const token_slice &tokens,                             //
    const bool is_inferred,                                //
    const bool has_rhs                                     //
) {
    token_slice tokens_mut = tokens;
    assert(!(is_inferred && !has_rhs));

    std::optional<DeclarationNode> declaration = std::nullopt;
    std::shared_ptr<Type> type;
    std::string name;

    token_slice lhs_tokens;
    if (has_rhs) {
        uint2 lhs_range = Matcher::get_next_match_range(tokens_mut, Matcher::until_eq_or_colon_equal).value();
        lhs_tokens = {tokens_mut.first + lhs_range.first, tokens_mut.first + lhs_range.second};
        tokens_mut.first = lhs_tokens.second;
    } else {
        lhs_tokens = tokens_mut;
    }

    // Remove all \n and \t from the lhs tokens
    remove_leading_garbage(lhs_tokens);

    // Check if the first token of the declaration is a `const` or `mut` token and set the mutability accordingly
    const bool is_mutable = lhs_tokens.first->type != TOK_CONST;
    if (lhs_tokens.first->type == TOK_CONST || lhs_tokens.first->type == TOK_MUT) {
        lhs_tokens.first++;
    }

    if (!has_rhs) {
        for (auto it = lhs_tokens.first; it != lhs_tokens.second; ++it) {
            if (std::next(it) != lhs_tokens.second && std::next(it)->type == TOK_SEMICOLON) {
                name = it->lexme;
                const long dist = std::distance(lhs_tokens.first, it);
                if (dist < 1) {
                    // No type declared
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                token_slice type_tokens = {lhs_tokens.first, it};
                if (!check_type_aliasing(type_tokens)) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                auto type_result = Type::get_type(type_tokens);
                if (!type_result.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                type = type_result.value();
                break;
            }
        }
        if (!scope->add_variable(name, type, scope->scope_id, is_mutable, false)) {
            // Variable shadowing
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, lhs_tokens.first->line, lhs_tokens.first->column, name);
            return std::nullopt;
        }
        std::optional<std::unique_ptr<ExpressionNode>> expr = std::nullopt;
        declaration = DeclarationNode(type, name, expr);
        return declaration;
    }

    if (lhs_tokens.first == lhs_tokens.second) {
        THROW_ERR(ErrStmtDanglingEqualSign, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }

    if (is_inferred) {
        auto expr = create_expression(scope, tokens_mut);
        if (!expr.has_value()) {
            THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens_mut);
            return std::nullopt;
        }
        if (dynamic_cast<const GroupType *>(expr.value()->type.get())) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        for (auto it = lhs_tokens.first; it != lhs_tokens.second; ++it) {
            if (it->type == TOK_IDENTIFIER && std::next(it)->type == TOK_COLON_EQUAL) {
                name = it->lexme;
                break;
            }
        }
        if (!scope->add_variable(name, expr.value()->type, scope->scope_id, is_mutable, false)) {
            // Variable shadowing
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, tokens_mut.first->line, tokens_mut.first->column, name);
            return std::nullopt;
        }
        declaration = DeclarationNode(expr.value()->type, name, expr);
    } else {
        token_slice type_tokens = {lhs_tokens.first, lhs_tokens.second - 2};
        lhs_tokens.first = type_tokens.second;
        if (!check_type_aliasing(type_tokens)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        auto type_result = Type::get_type(type_tokens);
        if (!type_result.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        type = type_result.value();
        name = lhs_tokens.first->lexme;
        if (!scope->add_variable(name, type, scope->scope_id, is_mutable, false)) {
            // Variable shadowing
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_name, tokens_mut.first->line, tokens_mut.first->column, name);
            return std::nullopt;
        }
        auto expr = create_expression(scope, tokens_mut, type);
        if (!expr.has_value()) {
            THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens_mut);
            return std::nullopt;
        }
        declaration = DeclarationNode(type, name, expr);
    }

    return declaration;
}

std::optional<UnaryOpStatement> Parser::create_unary_op_statement(Scope *scope, const token_slice &tokens) {
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

std::optional<DataFieldAssignmentNode> Parser::create_data_field_assignment(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    auto field_access_base = create_field_access_base(scope, tokens_mut);
    if (!field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now the equal sign should follow, we will delete that one too
    assert(tokens_mut.first->type == TOK_EQUAL);
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, tokens_mut);
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::string &var_name = std::get<1>(field_access_base.value());
    if (!std::get<2>(scope->variables.at(var_name))) {
        THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, tokens.first->line, tokens.first->column, var_name);
        return std::nullopt;
    }

    const auto &field_type = std::get<4>(field_access_base.value());
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
        expression.value() = std::make_unique<TypeCastNode>(field_type, expression.value());
    }

    return DataFieldAssignmentNode(             //
        std::get<0>(field_access_base.value()), // data_type
        var_name,                               // var_name
        std::get<2>(field_access_base.value()), // field_name
        std::get<3>(field_access_base.value()), // field_id
        field_type,                             // field_type
        expression.value()                      //
    );
}

std::optional<GroupedDataFieldAssignmentNode> Parser::create_grouped_data_field_assignment(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    auto grouped_field_access_base = create_grouped_access_base(scope, tokens_mut);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now the equal sign should follow, we will delete that one too
    assert(tokens_mut.first->type == TOK_EQUAL);
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, tokens_mut);
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::string &var_name = std::get<1>(grouped_field_access_base.value());
    if (!std::get<2>(scope->variables.at(var_name))) {
        THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, tokens.first->line, tokens.first->column, var_name);
        return std::nullopt;
    }

    return GroupedDataFieldAssignmentNode(              //
        std::get<0>(grouped_field_access_base.value()), // data_type
        var_name,                                       // var_name
        std::get<2>(grouped_field_access_base.value()), // field_names
        std::get<3>(grouped_field_access_base.value()), // field_ids
        std::get<4>(grouped_field_access_base.value()), // field_types
        expression.value()                              //
    );
}

std::optional<GroupedDataFieldAssignmentNode> Parser::create_grouped_data_field_assignment_shorthand( //
    Scope *scope,                                                                                     //
    const token_slice &tokens                                                                         //
) {
    token_slice tokens_mut = tokens;
    auto grouped_field_access_base = create_grouped_access_base(scope, tokens_mut);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Get the operation of the assignment shorthand
    Token operation = TOK_EOF;
    switch (tokens_mut.first->type) {
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
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, tokens_mut);
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Check if the data variable we try to mutate is marked as immutable
    const std::string &var_name = std::get<1>(grouped_field_access_base.value());
    if (!std::get<2>(scope->variables.at(var_name))) {
        THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, tokens.first->line, tokens.first->column, var_name);
        return std::nullopt;
    }

    // Create the lhs of the binary op of the right side, which is the grouped data field access
    std::unique_ptr<ExpressionNode> binop_lhs = std::make_unique<GroupedDataAccessNode>( //
        std::get<0>(grouped_field_access_base.value()),                                  // data_type
        var_name,                                                                        // var_name
        std::get<2>(grouped_field_access_base.value()),                                  // field_names
        std::get<3>(grouped_field_access_base.value()),                                  // field_ids
        std::get<4>(grouped_field_access_base.value())                                   // field_types
    );

    // The expression already is the rhs of the binop, so now we only need to check whether the types of the expression matches the types of
    // the assignment
    expression.value() = std::make_unique<BinaryOpNode>( //
        operation,                                       //
        binop_lhs,                                       //
        expression.value(),                              //
        binop_lhs->type,                                 //
        true                                             //
    );

    return GroupedDataFieldAssignmentNode(              //
        std::get<0>(grouped_field_access_base.value()), // data_type
        std::get<1>(grouped_field_access_base.value()), // var_name
        std::get<2>(grouped_field_access_base.value()), // field_names
        std::get<3>(grouped_field_access_base.value()), // field_ids
        std::get<4>(grouped_field_access_base.value()), // field_types
        expression.value()                              //
    );
}

std::optional<ArrayAssignmentNode> Parser::create_array_assignment(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    remove_leading_garbage(tokens_mut);
    // Now the first token should be the array identifier
    assert(tokens_mut.first->type == TOK_IDENTIFIER);
    const std::string variable_name = tokens_mut.first->lexme;
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
    assert(indexing_tokens.first->type == TOK_LEFT_BRACKET);
    indexing_tokens.first++;
    assert(std::prev(indexing_tokens.second)->type == TOK_RIGHT_BRACKET);
    indexing_tokens.second--;

    auto indexing_expressions = create_group_expressions(scope, indexing_tokens);
    if (!indexing_expressions.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Now the next token should be a = sign
    assert(tokens_mut.first->type == TOK_EQUAL);
    tokens_mut.first++;

    // Parse the rhs expression
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, tokens_mut, array_type->type);
    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }
    return ArrayAssignmentNode(variable_name, var_type.value(), array_type->type, indexing_expressions.value(), expression.value());
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_stacked_statement(Scope *scope, const token_slice &tokens) {
    token_list toks = clone_from_slice(tokens);

    if (!Matcher::tokens_contain(tokens, Matcher::token(TOK_EQUAL))) {
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    // Stacked statements cant be declarations, because sub-elements of a "stack" like `var.field.field` are already declared when one
    // can write a statement like this. This means the stacked statement is definitely an assignment First, find the position of the
    // equals sign
    auto iterator = tokens.first;
    while (iterator != tokens.second && iterator->type != TOK_EQUAL) {
        ++iterator;
    }
    assert(iterator->type == TOK_EQUAL);
    // Now, everything to the right of the iterator is the rhs expression
    const token_slice rhs_expr_tokens = {iterator + 1, tokens.second};
    auto rhs_expr = create_expression(scope, rhs_expr_tokens);
    if (!rhs_expr.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, rhs_expr_tokens);
        return std::nullopt;
    }
    --iterator;
    if (iterator->type != TOK_IDENTIFIER) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::string field_name = iterator->lexme;
    --iterator;
    if (iterator->type != TOK_DOT) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Okay, now everything to the left of the iterator is the base expression
    const token_slice base_expr_tokens = {tokens.first, iterator};
    auto base_expr = create_expression(scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, base_expr_tokens);
        return std::nullopt;
    }
    // The base expression should be a data type
    const DataType *base_expr_type = dynamic_cast<const DataType *>(base_expr.value()->type.get());
    if (base_expr_type == nullptr) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const DataNode *data_node = base_expr_type->data_node;
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

    auto stacked_assignment = std::make_unique<StackedAssignmentNode>(        //
        base_expr.value(), field_name, field_id, field_type, rhs_expr.value() //
    );
    return stacked_assignment;
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_statement(Scope *scope, const token_slice &tokens) {
    std::optional<std::unique_ptr<StatementNode>> statement_node = std::nullopt;

    if (Matcher::tokens_contain(tokens, Matcher::group_declaration_inferred)) {
        std::optional<GroupDeclarationNode> group_decl = create_group_declaration(scope, tokens);
        if (!group_decl.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupDeclarationNode>(std::move(group_decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::declaration_explicit)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, tokens, false, true);
        if (!decl.has_value()) {
            THROW_ERR(ErrStmtDeclarationCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::declaration_inferred)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, tokens, true, true);
        if (!decl.has_value()) {
            THROW_ERR(ErrStmtDeclarationCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::declaration_without_initializer)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, tokens, false, false);
        if (!decl.has_value()) {
            THROW_ERR(ErrStmtDeclarationCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::data_field_assignment)) {
        std::optional<DataFieldAssignmentNode> assign = create_data_field_assignment(scope, tokens);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<DataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::grouped_data_assignment)) {
        std::optional<GroupedDataFieldAssignmentNode> assign = create_grouped_data_field_assignment(scope, tokens);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupedDataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::grouped_data_assignment_shorthand)) {
        std::optional<GroupedDataFieldAssignmentNode> assign = create_grouped_data_field_assignment_shorthand(scope, tokens);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupedDataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::group_assignment)) {
        std::optional<GroupAssignmentNode> assign = create_group_assignment(scope, tokens);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::group_assignment_shorthand)) {
        std::optional<GroupAssignmentNode> assign = create_group_assignment_shorthand(scope, tokens);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::array_assignment)) {
        std::optional<ArrayAssignmentNode> assign = create_array_assignment(scope, tokens);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<ArrayAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::assignment)) {
        std::optional<AssignmentNode> assign = create_assignment(scope, tokens);
        if (!assign.has_value()) {
            THROW_ERR(ErrStmtAssignmentCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<AssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::assignment_shorthand)) {
        std::optional<AssignmentNode> assign = create_assignment_shorthand(scope, tokens);
        if (!assign.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        statement_node = std::make_unique<AssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::return_statement)) {
        std::optional<ReturnNode> return_node = create_return(scope, tokens);
        if (!return_node.has_value()) {
            THROW_ERR(ErrStmtReturnCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<ReturnNode>(std::move(return_node.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::throw_statement)) {
        std::optional<ThrowNode> throw_node = create_throw(scope, tokens);
        if (!throw_node.has_value()) {
            THROW_ERR(ErrStmtThrowCreationFailed, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        }
        statement_node = std::make_unique<ThrowNode>(std::move(throw_node.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::aliased_function_call)) {
        token_slice tokens_mut = tokens;
        remove_leading_garbage(tokens_mut);
        assert(tokens_mut.first->type == TOK_IDENTIFIER && std::next(tokens_mut.first)->type == TOK_DOT);
        const std::string alias_base = tokens_mut.first->lexme;
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
    } else {
        THROW_ERR(ErrStmtCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    return statement_node;
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_scoped_statement( //
    Scope *scope,                                                              //
    const token_slice &definition,                                             //
    token_slice &body,                                                         //
    std::vector<std::unique_ptr<StatementNode>> &statements                    //
) {
    std::optional<std::unique_ptr<StatementNode>> statement_node = std::nullopt;

    std::optional<unsigned int> indent_lvl_maybe = Matcher::get_leading_indents(                       //
        definition,                                                                                    //
        definition.first->type == TOK_EOL ? std::next(definition.first)->line : definition.first->line //
    );
    if (!indent_lvl_maybe.has_value()) {
        THROW_ERR(ErrMissingBody, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }
    token_slice scoped_body = get_body_tokens(indent_lvl_maybe.value(), body);
    body.first = scoped_body.second;

    if (Matcher::tokens_contain(definition, Matcher::if_statement) || Matcher::tokens_contain(definition, Matcher::else_if_statement) ||
        Matcher::tokens_contain(definition, Matcher::else_statement)) {
        std::vector<std::pair<token_slice, token_slice>> if_chain;
        if_chain.emplace_back(definition, scoped_body);
        if (Matcher::tokens_contain(definition, Matcher::token(TOK_ELSE))) {
            // else or else if at top of if chain
            THROW_ERR(ErrStmtIfChainMissingIf, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }

        token_slice next_definition = definition;
        while (true) {
            if (body.first == body.second) {
                break;
            }
            if (body.first->type == TOK_EOL) {
                body.first++;
            }
            // Get the next indices of the next definition
            std::optional<uint2> next_line_range = Matcher::get_tokens_line_range(body, body.first->line);
            if (!next_line_range.has_value()) {
                break;
            }
            // Check if the definition contains a 'else if' or 'else' statement. It cannot only contain a 'if' statement, as this then
            // will be part of its own IfNode and not part of this if-chain! This can be simplified to just check if the definition
            // contains a 'else' statement
            if (!Matcher::tokens_contain_in_range(body, Matcher::token(TOK_ELSE), next_line_range.value())) {
                break;
            }

            next_definition = {body.first + next_line_range.value().first, body.first + next_line_range.value().second};
            body.first = next_definition.second;
            scoped_body = get_body_tokens(indent_lvl_maybe.value(), body);
            body.first = scoped_body.second;
            if_chain.emplace_back(next_definition, scoped_body);
        }

        std::optional<std::unique_ptr<IfNode>> if_node = create_if(scope, if_chain);
        if (!if_node.has_value()) {
            THROW_ERR(ErrStmtIfCreationFailed, ERR_PARSING, file_name, if_chain);
            return std::nullopt;
        }
        statement_node = std::move(if_node.value());
    } else if (Matcher::tokens_contain(definition, Matcher::for_loop)) {
        std::optional<std::unique_ptr<ForLoopNode>> for_loop = create_for_loop(scope, definition, scoped_body);
        if (!for_loop.has_value()) {
            THROW_ERR(ErrStmtForCreationFailed, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        statement_node = std::move(for_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::par_for_loop) ||
        Matcher::tokens_contain(definition, Matcher::enhanced_for_loop)) {
        std::optional<std::unique_ptr<EnhForLoopNode>> enh_for_loop = create_enh_for_loop(scope, definition, scoped_body);
        if (!enh_for_loop.has_value()) {
            THROW_ERR(ErrStmtForCreationFailed, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        statement_node = std::move(enh_for_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::while_loop)) {
        std::optional<std::unique_ptr<WhileNode>> while_loop = create_while_loop(scope, definition, scoped_body);
        if (!while_loop.has_value()) {
            THROW_ERR(ErrStmtWhileCreationFailed, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        statement_node = std::move(while_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::catch_statement)) {
        std::optional<std::unique_ptr<CatchNode>> catch_node = create_catch(scope, definition, scoped_body, statements);
        if (!catch_node.has_value()) {
            THROW_ERR(ErrStmtCatchCreationFailed, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
        statement_node = std::move(catch_node.value());
    } else if (Matcher::tokens_contain(definition, Matcher::aliased_function_call)) {
        assert(definition.first->type == TOK_IDENTIFIER && std::next(definition.first)->type == TOK_DOT);
        const std::string alias_base = definition.first->lexme;
        statement_node = create_call_statement(scope, {definition.first + 2, definition.second}, alias_base);
    } else if (Matcher::tokens_contain(definition, Matcher::function_call)) {
        statement_node = create_call_statement(scope, definition, std::nullopt);
    } else {
        THROW_ERR(ErrStmtCreationFailed, ERR_PARSING, file_name, definition);
        return std::nullopt;
    }

    return statement_node;
}

std::optional<std::vector<std::unique_ptr<StatementNode>>> Parser::create_body(Scope *scope, const token_slice &body) {
    token_slice body_mut = body;
    std::vector<std::unique_ptr<StatementNode>> body_statements;

    std::optional<token_slice> for_tokens;
    while (auto next_match = Matcher::get_next_match_range(body_mut, Matcher::until_col_or_semicolon)) {
        token_slice statement_tokens = {body_mut.first + next_match.value().first, body_mut.first + next_match.value().second};
        body_mut.first = statement_tokens.second;
        if (Matcher::tokens_contain(statement_tokens, Matcher::token(TOK_FOR)) &&
            !Matcher::tokens_contain(statement_tokens, Matcher::token(TOK_IN))) {
            for_tokens = statement_tokens;
            continue;
        } else if (for_tokens.has_value()) {
            for_tokens.value().second = body_mut.first + next_match.value().second;
            if (!Matcher::tokens_contain(statement_tokens, Matcher::token(TOK_COLON))) {
                continue;
            } else {
                statement_tokens = for_tokens.value();
            }
        }
        std::optional<std::unique_ptr<StatementNode>> next_statement = std::nullopt;
        if (Matcher::tokens_contain(statement_tokens, Matcher::token(TOK_COLON))) {
            // --- SCOPED STATEMENT (IF, LOOPS, CATCH-BLOCK, SWITCH) ---
            next_statement = create_scoped_statement(scope, statement_tokens, body_mut, body_statements);
        } else {
            remove_leading_garbage(statement_tokens);
            if (Matcher::tokens_start_with(statement_tokens, Matcher::stacked_expression)) {
                // --- STACKED STATEMENT ---
                next_statement = create_stacked_statement(scope, statement_tokens);
            } else {
                // --- NORMAL STATEMENT ---
                next_statement = create_statement(scope, statement_tokens);
            }
        }

        if (next_statement.has_value()) {
            body_statements.emplace_back(std::move(next_statement.value()));
        } else {
            THROW_ERR(ErrStmtCreationFailed, ERR_PARSING, file_name, statement_tokens);
            return std::nullopt;
        }

        for_tokens = std::nullopt;
    }

    return body_statements;
}
