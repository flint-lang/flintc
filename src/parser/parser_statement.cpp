#include "error/error_type.hpp"
#include "parser/parser.hpp"

#include "error/error.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/switch_expression.hpp"
#include "parser/ast/expressions/switch_match_node.hpp"
#include "parser/ast/statements/break_node.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/ast/statements/callable_call_node_statement.hpp"
#include "parser/ast/statements/continue_node.hpp"
#include "parser/ast/statements/instance_call_node_statement.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/range_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"
#include "parser/type/vector_type.hpp"
#include "types.hpp"

#include <iterator>
#include <optional>
#include <string>
#include <variant>

std::optional<std::unique_ptr<StatementNode>> Parser::create_call_statement( //
    std::shared_ptr<Scope> &scope,                                           //
    const token_slice &tokens,                                               //
    const std::optional<Namespace *> &alias,                                 //
    const bool is_typed_call                                                 //
) {
    PROFILE_CUMULATIVE("Parser::create_call_statement");
    token_slice tokens_mut = tokens;
    std::optional<CreateCallOrInitializerBaseRet> ret = std::nullopt;
    if (alias.has_value()) {
        ret = create_call_or_initializer_base(_ctx_, scope, tokens_mut, alias.value(), is_typed_call);
    } else {
        ret = create_call_or_initializer_base(_ctx_, scope, tokens_mut, file_node_ptr->file_namespace.get(), is_typed_call);
    }
    if (!ret.has_value()) {
        return std::nullopt;
    }
    ASSERT(!ret->is_initializer);
    if (ret->instance_variable.has_value()) {
        ASSERT(ret->instance_variable.value()->get_variation() == ExpressionNode::Variation::VARIABLE);
        const VariableNode *instance_var = ret->instance_variable.value()->as<VariableNode>();
        if (scope->variables.find(instance_var->name) == scope->variables.end()) {
            THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_hash, instance_var->line, instance_var->column, instance_var->name);
            return std::nullopt;
        }
        if (!scope->variables.at(instance_var->name).is_mutable && !ret->function->is_const) {
            // Instance calls on constant instance variables are not allowed
            THROW_ERR(ErrExprCallOnConstInstance, ERR_PARSING, file_hash, tokens.first->line, tokens.first->column, instance_var->name);
            return std::nullopt;
        }
        std::unique_ptr<InstanceCallNodeStatement> instance_call_node = std::make_unique<InstanceCallNodeStatement>(           //
            file_hash, tokens, ret->function, ret->args, ret->function->error_types, ret->type, ret->instance_variable.value() //
        );
        instance_call_node->scope_id = scope->scope_id;
        last_parsed_call = instance_call_node.get();
        return std::move(instance_call_node);
    } else if (ret->callable.has_value()) {
        const auto &error_types = scope->variables.at(ret->callable.value()).type->as<FnType>()->error_types;
        std::unique_ptr<CallableCallNodeStatement> callable_call_node = std::make_unique<CallableCallNodeStatement>( //
            file_hash, tokens, ret->args, error_types, ret->type, ret->callable.value()                              //
        );
        callable_call_node->scope_id = scope->scope_id;
        last_parsed_call = callable_call_node.get();
        return std::move(callable_call_node);
    } else {
        std::unique_ptr<CallNodeStatement> simple_call_node = std::make_unique<CallNodeStatement>( //
            file_hash, tokens, ret->function, ret->args, ret->function->error_types, ret->type     //
        );
        simple_call_node->scope_id = scope->scope_id;
        last_parsed_call = simple_call_node.get();
        return std::move(simple_call_node);
    }
}

std::optional<ThrowNode> Parser::create_throw(std::shared_ptr<Scope> &scope, const token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::create_throw");
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
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(_ctx_, scope, expression_tokens);
    if (!expr.has_value()) {
        return std::nullopt;
    }
    if (expr.value()->type->get_variation() != Type::Variation::ERROR_SET && expr.value()->type->to_string() != "anyerror") {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, expression_tokens, Type::get_primitive_type("anyerror"), expr.value()->type);
        return std::nullopt;
    }
    if (expr.value()->get_variation() == ExpressionNode::Variation::VARIABLE) {
        const auto *var_node = expr.value()->as<VariableNode>();
        // Add the current scope to the scope list in the variable we throw to determine that the variable is returned, excluding it from
        // being freed to early
        std::vector<unsigned int> &ret_scopes = scope->variables.at(var_node->name).return_scope_ids;
        ret_scopes.emplace_back(scope->scope_id);
        // Also add all parent scope IDs so the variable is not freed at any ancestor scope level during cleanup
        std::shared_ptr<Scope> parent = scope->get_parent();
        while (parent != nullptr) {
            if (std::find(ret_scopes.begin(), ret_scopes.end(), parent->scope_id) == ret_scopes.end()) {
                ret_scopes.emplace_back(parent->scope_id);
            }
            parent = parent->get_parent();
        }
    }
    return ThrowNode(file_hash, tokens, expr.value());
}

std::optional<ReturnNode> Parser::create_return(std::shared_ptr<Scope> &scope, const token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::create_return");
    // Get the return type of the function
    std::shared_ptr<Type> return_type = scope->get_variable_type("flint.return_type").value();
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
        ASSERT(return_type->to_string() == "void");
        return ReturnNode(file_hash, tokens, return_expr);
    }
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(_ctx_, scope, expression_tokens);
    if (!expr.has_value()) {
        return std::nullopt;
    }
    if (expr.value()->get_variation() == ExpressionNode::Variation::VARIABLE) {
        const auto *variable_node = expr.value()->as<VariableNode>();
        std::shared_ptr<Scope> curr = scope;
        while (curr != nullptr) {
            auto it = curr->variables.find(variable_node->name);
            if (it != curr->variables.end()) {
                std::vector<unsigned int> &return_scopes = it->second.return_scope_ids;
                if (std::find(return_scopes.begin(), return_scopes.end(), curr->scope_id) == return_scopes.end()) {
                    return_scopes.emplace_back(curr->scope_id);
                }
                std::shared_ptr<Scope> ancestor = curr->get_parent();
                while (ancestor != nullptr) {
                    if (std::find(return_scopes.begin(), return_scopes.end(), ancestor->scope_id) == return_scopes.end()) {
                        return_scopes.emplace_back(ancestor->scope_id);
                    }
                    ancestor = ancestor->get_parent();
                }
            }
            curr = curr->get_parent();
        }
    }
    if (expr.value()->get_variation() == ExpressionNode::Variation::GROUP_EXPRESSION) {
        const auto *group_node = expr.value()->as<GroupExpressionNode>();
        for (auto &group_expr : group_node->expressions) {
            if (group_expr->get_variation() == ExpressionNode::Variation::VARIABLE) {
                const auto *variable_node = group_expr->as<VariableNode>();
                std::shared_ptr<Scope> curr = scope;
                while (curr != nullptr) {
                    auto it = curr->variables.find(variable_node->name);
                    if (it != curr->variables.end()) {
                        std::vector<unsigned int> &return_scopes = it->second.return_scope_ids;
                        if (std::find(return_scopes.begin(), return_scopes.end(), curr->scope_id) == return_scopes.end()) {
                            return_scopes.emplace_back(curr->scope_id);
                        }
                        std::shared_ptr<Scope> ancestor = curr->get_parent();
                        while (ancestor != nullptr) {
                            if (std::find(return_scopes.begin(), return_scopes.end(), ancestor->scope_id) == return_scopes.end()) {
                                return_scopes.emplace_back(ancestor->scope_id);
                            }
                            ancestor = ancestor->get_parent();
                        }
                    }
                    curr = curr->get_parent();
                }
            }
        }
    }
    if (!check_castability(return_type, expr.value())) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    return ReturnNode(file_hash, tokens, expr);
}

std::optional<std::unique_ptr<IfNode>> Parser::create_if(            //
    std::shared_ptr<Scope> &scope,                                   //
    const unsigned int scope_segment,                                //
    std::vector<std::pair<token_slice, std::vector<Line>>> &if_chain //
) {
    PROFILE_CUMULATIVE("Parser::create_if");
    ASSERT(!if_chain.empty());
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
    if (has_else && !has_if) {
        UNREACHABLE();
        return std::nullopt;
    }

    // Create the if statements condition and body statements
    std::optional<std::unique_ptr<ExpressionNode>> condition = create_expression( //
        _ctx_, scope, this_if_pair.first, Type::get_primitive_type("bool")        //
    );
    if (!condition.has_value()) {
        // Invalid expression inside if statement
        return std::nullopt;
    }
    if (condition.value()->type->to_string() != "bool") {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, this_if_pair.first, Type::get_primitive_type("bool"),
            condition.value()->type);
        return std::nullopt;
    }
    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(scope, scope_segment);
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
            else_scope = create_if(scope, scope_segment, if_chain);
        } else {
            // 'else'
            if (if_chain.size() > 1) {
                // Dangling else or else if statement after last else statement
                THROW_ERR(ErrStmtDanglingElse, ERR_PARSING, file_hash, if_chain.at(1).first);
                return std::nullopt;
            }
            std::shared_ptr<Scope> else_scope_ptr = std::make_shared<Scope>(scope, scope_segment);
            auto else_body_statements = create_body(else_scope_ptr, if_chain.front().second);
            if (!else_body_statements.has_value()) {
                return std::nullopt;
            }
            else_scope_ptr->body = std::move(else_body_statements.value());
            else_scope = std::move(else_scope_ptr);
        }
    }

    unsigned int end_line = this_if_pair.second.back().tokens.second->line;
    if (else_scope.has_value()) {
        if (std::holds_alternative<std::shared_ptr<Scope>>(else_scope.value())) {
            const auto &else_body = std::get<std::shared_ptr<Scope>>(else_scope.value())->body;
            if (!else_body.empty() && else_body.back()->line > end_line) {
                end_line = else_body.back()->line;
            }
        } else {
            const auto &else_if = *std::get<std::unique_ptr<IfNode>>(else_scope.value());
            unsigned int else_end = else_if.end_line != 0 ? else_if.end_line : else_if.line;
            if (else_end > end_line) {
                end_line = else_end;
            }
        }
    }
    auto if_node = std::make_unique<IfNode>(file_hash, this_if_pair.first, condition.value(), body_scope, else_scope);
    if_node->end_line = end_line;
    return if_node;
}

std::optional<std::unique_ptr<DoWhileNode>> Parser::create_do_while_loop( //
    std::shared_ptr<Scope> &scope,                                        //
    const unsigned int scope_segment,                                     //
    const token_slice &condition_line,                                    //
    const std::vector<Line> &body                                         //
) {
    PROFILE_CUMULATIVE("Parser::create_do_while_loop");
    token_slice condition_tokens = condition_line;
    // Remove everything in front of the expression (\n, \t, else, if)
    for (auto it = condition_tokens.first; it != condition_tokens.second; ++it) {
        if (it->token == TOK_WHILE) {
            condition_tokens.first++;
            break;
        }
        condition_tokens.first++;
    }
    // Remove everything after the expression (;, \n)
    for (auto rev_it = std::prev(condition_tokens.second); rev_it != condition_tokens.first; --rev_it) {
        if (rev_it->token == TOK_SEMICOLON) {
            condition_tokens.second--;
            break;
        }
        condition_tokens.second--;
    }

    std::optional<std::unique_ptr<ExpressionNode>> condition = create_expression( //
        _ctx_, scope, condition_tokens, Type::get_primitive_type("bool")          //
    );
    if (!condition.has_value()) {
        // Invalid expression inside while statement
        return std::nullopt;
    }

    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(scope, scope_segment);
    auto body_statements = create_body(body_scope, body);
    if (!body_statements.has_value()) {
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());
    std::unique_ptr<DoWhileNode> do_while_node = std::make_unique<DoWhileNode>(file_hash, condition_line, condition.value(), body_scope);
    return do_while_node;
}

std::optional<std::unique_ptr<WhileNode>> Parser::create_while_loop( //
    std::shared_ptr<Scope> &scope,                                   //
    const unsigned int scope_segment,                                //
    const token_slice &definition,                                   //
    const std::vector<Line> &body                                    //
) {
    PROFILE_CUMULATIVE("Parser::create_while_loop");
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

    std::optional<std::unique_ptr<ExpressionNode>> condition = create_expression( //
        _ctx_, scope, condition_tokens, Type::get_primitive_type("bool")          //
    );
    if (!condition.has_value()) {
        // Invalid expression inside while statement
        return std::nullopt;
    }

    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(scope, scope_segment);
    auto body_statements = create_body(body_scope, body);
    if (!body_statements.has_value()) {
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());
    std::unique_ptr<WhileNode> while_node = std::make_unique<WhileNode>(file_hash, definition, condition.value(), body_scope);
    return while_node;
}

std::optional<std::unique_ptr<ForLoopNode>> Parser::create_for_loop( //
    std::shared_ptr<Scope> &scope,                                   //
    const unsigned int scope_segment,                                //
    const token_slice &definition,                                   //
    const std::vector<Line> &body                                    //
) {
    PROFILE_CUMULATIVE("Parser::create_for_loop");
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
    std::shared_ptr<Scope> definition_scope = std::make_shared<Scope>(scope, scope_segment);
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
    initializer = create_statement(definition_scope, scope_segment, initializer_tokens);
    if (!initializer.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    definition_scope->body.emplace_back(std::move(initializer.value()));

    // Parse the loop condition expression
    uint2 &condition_range = expression_ranges.at(1);
    const token_slice condition_tokens = {definition.first + condition_range.first, definition.first + condition_range.second};
    condition = create_expression(_ctx_, definition_scope, condition_tokens);
    if (!condition.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Parse the for loops body
    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(definition_scope, scope_segment);
    auto body_statements = create_body(body_scope, body);
    if (!body_statements.has_value()) {
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());

    // Parse the looparound statement. The looparound statement is actually part of the body is the last statement of the body
    token_slice looparound_tokens = {definition.first + condition_range.second, definition.first + expressions_range.value().second};
    looparound = create_statement(body_scope, scope_segment, looparound_tokens);
    if (!looparound.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    auto for_node = std::make_unique<ForLoopNode>(                                                 //
        file_hash, definition, condition.value(), definition_scope, looparound.value(), body_scope //
    );
    return for_node;
}

std::optional<std::unique_ptr<EnhForLoopNode>> Parser::create_enh_for_loop( //
    std::shared_ptr<Scope> &scope,                                          //
    const unsigned int scope_segment,                                       //
    const token_slice &definition,                                          //
    const std::vector<Line> &body                                           //
) {
    PROFILE_CUMULATIVE("Parser::create_enh_for_loop");
    token_slice definition_mut = definition;
    remove_trailing_garbage(definition_mut);

    // Now the first token should be the `for` token
    ASSERT(definition_mut.first->token == TOK_FOR);
    definition_mut.first++;

    // The next token should either be a `(` or an identifer. If its an identifier we use the "tupled" enhanced for loop approach
    std::variant<std::pair<std::optional<std::string>, std::optional<std::string>>, std::string> iterators;
    if (definition_mut.first->token == TOK_IDENTIFIER) {
        // Its a tuple, e.g. `for t in iterable:` where `t` is of type `data<u64, T>`
        iterators = std::string(definition_mut.first->lexme);
        definition_mut.first++;
    } else {
        // Its a group, e.g. `for (index, element) in iterable:`
        ASSERT(definition_mut.first->token == TOK_LEFT_PAREN);
        definition_mut.first++;
        std::optional<std::string> index_identifier;
        if (definition_mut.first->token == TOK_IDENTIFIER) {
            index_identifier = std::string(definition_mut.first->lexme);
        }
        definition_mut.first++;
        ASSERT(definition_mut.first->token == TOK_COMMA);
        definition_mut.first++;
        std::optional<std::string> element_identifier;
        if (definition_mut.first->token == TOK_IDENTIFIER) {
            element_identifier = std::string(definition_mut.first->lexme);
        }
        definition_mut.first++;
        ASSERT(definition_mut.first->token == TOK_RIGHT_PAREN);
        definition_mut.first++;
        iterators = std::make_pair(index_identifier, element_identifier);
    }
    ASSERT(definition_mut.first->token == TOK_IN);
    definition_mut.first++;

    // Create the definition scope
    std::shared_ptr<Scope> definition_scope = std::make_shared<Scope>(scope, scope_segment);

    // The rest of the definition is the iterable expression
    std::optional<std::unique_ptr<ExpressionNode>> iterable = create_expression(_ctx_, definition_scope, definition_mut);
    if (!iterable.has_value()) {
        return std::nullopt;
    }
    // Now that the iterable is parsed we know its type and we can check if its an iterable. For now, only arrays, strings and ranges are
    // considered as being iterable
    std::shared_ptr<Type> element_type;
    switch (iterable.value()->type->get_variation()) {
        default:
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        case Type::Variation::ARRAY: {
            const auto *type = iterable.value()->type->as<ArrayType>();
            element_type = type->type;
            break;
        }
        case Type::Variation::PRIMITIVE: {
            const auto *type = iterable.value()->type->as<PrimitiveType>();
            if (type->type_name != "str") {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            element_type = Type::get_primitive_type("u8");
            break;
        }
        case Type::Variation::RANGE: {
            const auto *type = iterable.value()->type->as<RangeType>();
            element_type = type->bound_type;
            break;
        }
    }

    // Add the variable(s) to the definition scope
    if (std::holds_alternative<std::string>(iterators)) {
        // We add the tuple variable to the definition scope
        const std::string tuple_name = std::get<std::string>(iterators);
        std::vector<std::shared_ptr<Type>> tuple_types = {Type::get_primitive_type("u64"), element_type};
        std::shared_ptr<Type> tuple_type = std::make_shared<TupleType>(tuple_types);
        if (!file_node_ptr->file_namespace->add_type(tuple_type)) {
            tuple_type = file_node_ptr->file_namespace->get_type_from_str(tuple_type->to_string()).value();
        }
        if (!definition_scope->add_variable(tuple_name,
                Scope::Variable{
                    .type = tuple_type,
                    .scope_id = definition_scope->scope_id,
                    .scope_segment = scope_segment,
                    .is_mutable = false,
                    .is_persistent = false,
                    .is_fn_param = true,
                    .is_reference = true,
                    .return_scope_ids = {},
                    .is_pseudo_variable = false,
                    .file_hash = file_hash,
                    .line = (definition_mut.first - 2)->line,
                    .column = (definition_mut.first - 2)->column,
                }) //
        ) {
            auto tuple_it = definition_mut.first - 2;
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_hash, tuple_it->line, tuple_it->column, tuple_name);
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
            if (!definition_scope->add_variable(index_name.value(),
                    Scope::Variable{
                        .type = index_type,
                        .scope_id = definition_scope->scope_id,
                        .scope_segment = scope_segment,
                        .is_mutable = false,
                        .is_persistent = false,
                        .is_fn_param = false,
                        .is_reference = true,
                        .return_scope_ids = {},
                        .is_pseudo_variable = false,
                        .file_hash = file_hash,
                        .line = index_it->line,
                        .column = index_it->column,
                    }) //
            ) {
                THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_hash, index_it->line, index_it->column, index_name.value());
                return std::nullopt;
            }
        }
        if (element_name.has_value()) {
            auto element_it = definition_mut.first - 3;
            if (!definition_scope->add_variable(element_name.value(),
                    Scope::Variable{
                        .type = element_type,
                        .scope_id = definition_scope->scope_id,
                        .scope_segment = scope_segment,
                        .is_mutable = true,
                        .is_persistent = false,
                        .is_fn_param = false,
                        .is_reference = true,
                        .return_scope_ids = {},
                        .is_pseudo_variable = false,
                        .file_hash = file_hash,
                        .line = element_it->line,
                        .column = element_it->column,
                    }) //
            ) {
                THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_hash, element_it->line, element_it->column, element_name.value());
                return std::nullopt;
            }
        }
    }

    // Now create the body scope and parse the body
    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(definition_scope, scope_segment);
    auto body_statements = create_body(body_scope, body);
    if (!body_statements.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    body_scope->body = std::move(body_statements.value());

    auto enh_for_node = std::make_unique<EnhForLoopNode>(                                //
        file_hash, definition, iterators, iterable.value(), definition_scope, body_scope //
    );
    return enh_for_node;
}

bool Parser::create_switch_branch_body(                              //
    std::shared_ptr<Scope> &scope,                                   //
    const unsigned int scope_segment,                                //
    std::vector<std::unique_ptr<ExpressionNode>> &match_expressions, //
    std::vector<SSwitchBranch> &s_branches,                          //
    std::vector<ESwitchBranch> &e_branches,                          //
    std::vector<Line>::const_iterator &line_it,                      //
    const std::vector<Line> &body,                                   //
    const token_slice &tokens,                                       //
    const uint2 &match_range,                                        //
    const bool is_statement                                          //
) {
    PROFILE_CUMULATIVE("Parser::create_switch_branch_body");
    if (!is_statement) {
        // When it's a switch expression, no body will follow, ever. Only expressions are allowed to the right of the arrow, so we
        // can parse the rhs as an expression
        ASSERT(std::prev(tokens.second)->token == TOK_SEMICOLON);
        const token_slice expression_tokens = {tokens.first + match_range.second, tokens.second - 1};
        auto expression = create_expression(_ctx_, scope, expression_tokens);
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
    std::shared_ptr<Scope> branch_body = std::make_shared<Scope>(scope, scope_segment);
    if (tokens.first + match_range.second != tokens.second) {
        // A single statement follows, this means that the line needs to end with a semicolon
        const token_slice statement_tokens = {tokens.first + match_range.second, tokens.second};
        if (std::prev(statement_tokens.second)->token != TOK_SEMICOLON) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        auto statement = create_statement(branch_body, scope_segment, statement_tokens);
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
    ASSERT(tokens.first + match_range.second == tokens.second);
    ASSERT(std::prev(tokens.second)->token == TOK_COLON);
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
    std::shared_ptr<Scope> &scope,              //
    const unsigned int scope_segment,           //
    std::vector<SSwitchBranch> &s_branches,     //
    std::vector<ESwitchBranch> &e_branches,     //
    const std::vector<Line> &body,              //
    const std::shared_ptr<Type> &switcher_type, //
    const bool is_statement                     //
) {
    PROFILE_CUMULATIVE("Parser::create_switch_branches");
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
            matches.emplace_back(std::make_unique<DefaultNode>(file_hash, get_pos_triple(match_tokens), switcher_type));
        } else {
            auto match_expressions = create_group_expressions(_ctx_, scope, match_tokens);
            if (!match_expressions.has_value()) {
                return false;
            }
            matches = std::move(match_expressions.value());
        }
        if (matches.empty()) {
            return false;
        }
        if (switcher_type->get_variation() != Type::Variation::PRIMITIVE) {
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return false;
        }
        for (auto &match : matches) {
            const auto variation = match->get_variation();
            const bool is_not_literal = variation != ExpressionNode::Variation::LITERAL;
            const bool is_not_default_value = variation != ExpressionNode::Variation::DEFAULT;
            if (is_not_literal && is_not_default_value) {
                // Not allowed value for the switch statement's expression
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            if (!check_castability(switcher_type, match)) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
        }
        if (!create_switch_branch_body(                                                                                          //
                scope, scope_segment, matches, s_branches, e_branches, line_it, body, tokens, match_range.value(), is_statement) //
        ) {
            return false;
        }
    }
    return true;
}

bool Parser::create_enum_switch_branches(       //
    std::shared_ptr<Scope> &scope,              //
    const unsigned int scope_segment,           //
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
    const std::vector<std::pair<std::string, unsigned int>> &enum_values = enum_node->values;
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
                match_expressions.push_back(std::make_unique<DefaultNode>(file_hash, get_pos_triple(match_tokens), switcher_type));
            } else {
                const std::string enum_value(match_tokens.first->lexme);
                const std::vector<std::string>::const_iterator matched_enum_id = std::find( //
                    matched_enum_values.begin(), matched_enum_values.end(), enum_value      //
                );
                if (matched_enum_id != matched_enum_values.end()) {
                    // Duplicate branch enum
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                bool enum_contains_tag = false;
                for (size_t i = 0; i < enum_values.size(); i++) {
                    if (enum_values.at(i).first == enum_value) {
                        enum_contains_tag = true;
                        break;
                    }
                }
                if (!enum_contains_tag) {
                    // Enum value not part of the enum values
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                matched_enum_values.push_back(enum_value);
                LitValue lit_value = LitEnum{switcher_type, std::vector<std::string>{enum_value}};
                match_expressions.push_back(                                                                         //
                    std::make_unique<LiteralNode>(file_hash, get_pos_triple(match_tokens), lit_value, switcher_type) //
                );
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
                const std::vector<std::string>::const_iterator enum_id = std::find(    //
                    matched_enum_values.begin(), matched_enum_values.end(), enum_value //
                );
                if (enum_id != matched_enum_values.end()) {
                    // Duplicate branch enum
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                bool enum_contains_tag = false;
                for (size_t i = 0; i < enum_values.size(); i++) {
                    if (enum_values.at(i).first == enum_value) {
                        enum_contains_tag = true;
                        break;
                    }
                }
                if (!enum_contains_tag) {
                    // Enum value not part of the enum values
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                matched_enum_values.push_back(enum_value);
                LitValue lit_value = LitEnum{switcher_type, std::vector<std::string>{enum_value}};
                match_expressions.push_back(                                                                         //
                    std::make_unique<LiteralNode>(file_hash, get_pos_triple(match_tokens), lit_value, switcher_type) //
                );
            }
        }
        if (!create_switch_branch_body(                                          //
                scope, scope_segment, match_expressions, s_branches, e_branches, //
                line_it, body, tokens, match_range.value(), is_statement)        //
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
    std::shared_ptr<Scope> &scope,              //
    const unsigned int scope_segment,           //
    std::vector<SSwitchBranch> &s_branches,     //
    std::vector<ESwitchBranch> &e_branches,     //
    const std::vector<Line> &body,              //
    const std::shared_ptr<Type> &switcher_type, //
    const ErrorNode *error_node,                //
    const bool is_statement                     //
) {
    PROFILE_CUMULATIVE("Parser::create_error_switch_branches");
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
                match_expressions.push_back(std::make_unique<DefaultNode>(file_hash, get_pos_triple(match_tokens), switcher_type));
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
                match_expressions.push_back(
                    std::make_unique<LiteralNode>(file_hash, get_pos_triple(match_tokens), lit_value, switcher_type));
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
                match_expressions.push_back(
                    std::make_unique<LiteralNode>(file_hash, get_pos_triple(match_tokens), lit_value, switcher_type));
            }
        }
        if (!create_switch_branch_body(                                          //
                scope, scope_segment, match_expressions, s_branches, e_branches, //
                line_it, body, tokens, match_range.value(), is_statement)        //
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
    std::shared_ptr<Scope> &scope,              //
    const unsigned int scope_segment,           //
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
            match_expressions.push_back(std::make_unique<LiteralNode>(file_hash, get_pos_triple(match_tokens), lit_value, switcher_type));
            if (!create_switch_branch_body(                                          //
                    scope, scope_segment, match_expressions, s_branches, e_branches, //
                    line_it, body, tokens, match_range.value(), is_statement)        //
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
            const auto *optional_type = switcher_type->as<OptionalType>();
            const std::string match_str(match_tokens.first->lexme);
            match_expressions.push_back(                                                                                           //
                std::make_unique<SwitchMatchNode>(file_hash, get_pos_triple(match_tokens), optional_type->base_type, match_str, 1) //
            );
            std::shared_ptr<Scope> branch_scope = std::make_shared<Scope>(scope, scope_segment);
            const unsigned int scope_id = branch_scope->scope_id;
            const std::string var_name(match_tokens.first->lexme);
            if (!branch_scope->add_variable(var_name,
                    {
                        .type = optional_type->base_type,
                        .scope_id = scope_id,
                        .scope_segment = scope_segment,
                        .is_mutable = is_mutable,
                        .is_persistent = false,
                        .is_fn_param = true,
                        .file_hash = file_hash,
                        .line = match_tokens.first->line,
                        .column = match_tokens.first->column,
                    })) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            if (!create_switch_branch_body(                                                 //
                    branch_scope, scope_segment, match_expressions, s_branches, e_branches, //
                    line_it, body, tokens, match_range.value(), is_statement)               //
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
    std::shared_ptr<Scope> &scope,              //
    const unsigned int scope_segment,           //
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
    const auto *variant_type = switcher_type->as<VariantType>();
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
                match_expressions.push_back(std::make_unique<DefaultNode>(file_hash, get_pos_triple(match_tokens), switcher_type));
                if (!create_switch_branch_body(                                          //
                        scope, scope_segment, match_expressions, s_branches, e_branches, //
                        line_it, body, tokens, match_range.value(), is_statement)        //
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

        if (match_tokens.first->type->equals(switcher_type)) {
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
                if (type_it->second->equals(match_tokens.first->type) && !type_it->first.has_value()) {
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
        if ((++match_tokens.first)->token != TOK_LEFT_PAREN) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const std::shared_ptr<Type> &access_type = possible_types.at(type_idx - 1).second;
        std::optional<std::string> access_name;
        if (access_type->to_string() == "void") {
            // Void typed tags are not allowed to have an accessor
            if ((match_tokens.first + 1)->token == TOK_IDENTIFIER) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
        } else {
            // Non-void typed tags must have an accessor
            if ((match_tokens.first + 1)->token != TOK_IDENTIFIER) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            access_name = std::string((++match_tokens.first)->lexme);
        }
        if ((++match_tokens.first)->token != TOK_RIGHT_PAREN) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        match_expressions.push_back(                                                                                       //
            std::make_unique<SwitchMatchNode>(file_hash, get_pos_triple(match_tokens), access_type, access_name, type_idx) //
        );
        std::shared_ptr<Scope> branch_scope = std::make_shared<Scope>(scope, scope_segment);
        if (access_name.has_value()) {
            if (!branch_scope->add_variable(access_name.value(),
                    {
                        .type = access_type,
                        .scope_id = branch_scope->scope_id,
                        .scope_segment = scope_segment,
                        .is_mutable = is_mutable,
                        .is_persistent = false,
                        .is_fn_param = true,
                        .file_hash = file_hash,
                        .line = match_tokens.first->line,
                        .column = match_tokens.first->column,
                    })) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
        }
        if (!create_switch_branch_body(                                                 //
                branch_scope, scope_segment, match_expressions, s_branches, e_branches, //
                line_it, body, tokens, match_range.value(), is_statement)               //
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
    std::shared_ptr<Scope> &scope,                                             //
    const unsigned int scope_segment,                                          //
    const token_slice &definition,                                             //
    const std::vector<Line> &body                                              //
) {
    // First, check if the definition starts with a switch token. If it starts with a switch token it's a switch statement, otherwise
    // it's a switch expression
    token_slice switcher_tokens = definition;
    const bool is_statement = switcher_tokens.first->token == TOK_SWITCH;
    ASSERT(std::prev(switcher_tokens.second)->token == TOK_COLON);
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
    std::optional<std::unique_ptr<ExpressionNode>> switcher = create_expression(_ctx_, scope, switcher_tokens);
    if (!switcher.has_value()) {
        return std::nullopt;
    }
    switch (switcher.value()->type->get_variation()) {
        default:
            if (!create_switch_branches(scope, scope_segment, s_branches, e_branches, body, switcher.value()->type, is_statement)) {
                return std::nullopt;
            }
            break;
        case Type::Variation::ENUM: {
            const auto *type = switcher.value()->type->as<EnumType>();
            if (!create_enum_switch_branches(                                                                                  //
                    scope, scope_segment, s_branches, e_branches, body, switcher.value()->type, type->enum_node, is_statement) //
            ) {
                return std::nullopt;
            }
            break;
        }
        case Type::Variation::ERROR_SET: {
            const auto *type = switcher.value()->type->as<ErrorSetType>();
            const ErrorNode *error_node = type->error_node;
            if (!create_error_switch_branches(                                                                            //
                    scope, scope_segment, s_branches, e_branches, body, switcher.value()->type, error_node, is_statement) //
            ) {
                return std::nullopt;
            }
            break;
        }
        case Type::Variation::OPTIONAL: {
            const auto *var_node = switcher.value()->as<VariableNode>();
            const bool is_mutable = scope->variables.at(var_node->name).is_mutable;
            if (!create_optional_switch_branches(                                                                         //
                    scope, scope_segment, s_branches, e_branches, body, switcher.value()->type, is_statement, is_mutable) //
            ) {
                return std::nullopt;
            }
            break;
        }
        case Type::Variation::VARIANT: {
            if (switcher.value()->get_variation() != ExpressionNode::Variation::VARIABLE) {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            }
            const auto *var_node = switcher.value()->as<VariableNode>();
            const bool is_mutable = scope->variables.at(var_node->name).is_mutable;
            if (!create_variant_switch_branches(                                                                          //
                    scope, scope_segment, s_branches, e_branches, body, switcher.value()->type, is_statement, is_mutable) //
            ) {
                return std::nullopt;
            }
            break;
        }
    }
    if (is_statement) {
        auto switch_node = std::make_unique<SwitchStatement>(file_hash, definition, switcher.value(), s_branches);
        return switch_node;
    }
    // Because it's an expression which contains the switch expression as it's rhs, we still need to parse everything to the left of the
    // switch and pass the switch as the rhs expression to it.
    ASSERT(!e_branches.empty());
    // Check if all branch expressions share the same common type, throw an error if they do not. A common type for example would be when
    // one branch is of type `i32`, the next of type `int` (comptime type) and the next of type `i64`, then the common type would be `i64`.
    // A common type is definted to be a type to which all branches can be implicitely cast to or be equal to that type.
    // The first type *is* the common type, and then we go through all types, if we find a type which does not match the common type we
    // check if we can cast it to the common type. If we can not cast to it we simply assume it to be the next common type, but the old
    // common type needs to be castable to the new type. If this conditions does not hold true we throw an error. Implicit casting follows
    // the rules of transivity so if A is castable to B and B is castable to C then A is castable to C too, we can use this to our advantage
    // here.
    std::shared_ptr<Type> common_type = e_branches.front().expr->type;
    for (const auto &branch : e_branches) {
        if (branch.expr->type == common_type) {
            continue;
        }

        CastDirection cast_dir = check_primitive_castability(common_type, branch.expr->type);
        switch (cast_dir.kind) {
            case CastDirection::Kind::NOT_CASTABLE:
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            case CastDirection::Kind::SAME_TYPE:
                continue;
            case CastDirection::Kind::CAST_LHS_TO_RHS:
                // Common type casts to branch type - branch type is "wider"
                common_type = branch.expr->type;
                break;
            case CastDirection::Kind::CAST_BIDIRECTIONAL: // Keep the common type since the rhs can cast to it too
            case CastDirection::Kind::CAST_RHS_TO_LHS:
                // Branch type casts to common type - keep common type
                break;
            case CastDirection::Kind::CAST_BOTH_TO_COMMON:
                // Both cast to a third type (e.g., i32 + float → f32)
                common_type = cast_dir.common_type;
                break;
        }
    }
    // Cast all branch expressions to the common type, only if the expression's type differs from the common type
    for (auto &branch : e_branches) {
        if (!check_castability(common_type, branch.expr, true)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }
    auto switch_expr = std::make_unique<SwitchExpression>(file_hash, get_pos_triple(definition), switcher.value(), e_branches);
    // Now we need to parse the lhs of the switch *somehow*...
    token_slice lhs_tokens = {definition.first, switcher_tokens.first - 1};
    ASSERT(lhs_tokens.second->token == TOK_SWITCH);
    auto whole_statement = create_statement(scope, scope_segment, lhs_tokens, std::move(switch_expr));
    if (!whole_statement.has_value()) {
        return std::nullopt;
    }
    return whole_statement;
}

std::optional<std::unique_ptr<CatchNode>> Parser::create_catch( //
    std::shared_ptr<Scope> &scope,                              //
    const unsigned int scope_segment,                           //
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
    ASSERT(catch_id.has_value());
    // A call is three tokens minimum: identifier(), so everything smaller than that means the catch stands alone
    if (catch_id.value() < definition.first + 3) {
        THROW_ERR(ErrStmtDanglingCatch, ERR_PARSING, file_hash, definition);
        return std::nullopt;
    }

    token_slice left_of_catch = {definition.first, catch_id.value()};
    std::optional<std::unique_ptr<StatementNode>> lhs = create_statement(scope, scope_segment, left_of_catch);
    if (!lhs.has_value()) {
        return std::nullopt;
    }
    statements.emplace_back(std::move(lhs.value()));
    // Get the last parsed call and set the 'has_catch' property of the call node
    if (!last_parsed_call.has_value()) {
        THROW_ERR(ErrStmtDanglingCatch, ERR_PARSING, file_hash, definition);
        return std::nullopt;
    }
    CallNodeBase *catch_base_call = last_parsed_call.value();
    catch_base_call->has_catch = true;

    const token_slice right_of_catch = {catch_id.value(), std::prev(definition.second)};
    std::optional<std::string> err_var = std::nullopt;
    token_list::iterator err_var_token = right_of_catch.first;
    for (auto it = right_of_catch.first; it != right_of_catch.second; ++it) {
        if (it->token == TOK_CATCH && std::next(it) != right_of_catch.second && std::next(it)->token == TOK_IDENTIFIER) {
            err_var = std::next(it)->lexme;
            err_var_token = std::next(it);
        }
    }

    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>(scope, scope_segment);
    if (err_var.has_value()) {
        // Get all the possible error types of the call and give the error value a type depending on them
        const auto &error_types = catch_base_call->error_types;
        std::shared_ptr<Type> err_variable_type;
        if (error_types.size() == 1) {
            err_variable_type = Type::get_primitive_type("anyerror");
        } else {
            const std::variant<VariantNode *const, std::vector<std::shared_ptr<Type>>> var_or_list = error_types;
            err_variable_type = std::make_shared<VariantType>(var_or_list, true);
            if (!file_node_ptr->file_namespace->add_type(err_variable_type)) {
                err_variable_type = file_node_ptr->file_namespace->get_type_from_str(err_variable_type->to_string()).value();
            }
        }
        if (!body_scope->add_variable(err_var.value(),
                Scope::Variable{
                    .type = err_variable_type,
                    .scope_id = body_scope->scope_id,
                    .scope_segment = scope_segment,
                    .is_mutable = false,
                    .is_persistent = false,
                    .is_fn_param = false,
                    .is_reference = false,
                    .return_scope_ids = {},
                    .is_pseudo_variable = false,
                    .file_hash = file_hash,
                    .line = err_var_token->line,
                    .column = err_var_token->column,
                }) //
        ) {
            THROW_ERR(                                                                                                                //
                ErrVarRedefinition, ERR_PARSING, file_hash, right_of_catch.first->line, right_of_catch.first->column, err_var.value() //
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
        if (!file_node_ptr->file_namespace->add_type(switcher_type)) {
            switcher_type = file_node_ptr->file_namespace->get_type_from_str(switcher_type->to_string()).value();
        }
        if (!body_scope->add_variable("flint.value_err", {switcher_type, body_scope->scope_id, scope_segment, false, false, false})) {
            UNREACHABLE();
            return std::nullopt;
        }
        if (!create_variant_switch_branches(body_scope, scope_segment, s_branches, e_branches, body, switcher_type, true, false)) {
            return std::nullopt;
        }
        std::unique_ptr<ExpressionNode> dummy_switcher = std::make_unique<VariableNode>(      //
            file_hash, get_pos_triple(right_of_catch), "flint.value_err", switcher_type, true //
        );
        std::unique_ptr<StatementNode> switch_statement = std::make_unique<SwitchStatement>( //
            file_hash, definition, dummy_switcher, s_branches                                //
        );
        body_scope->body.push_back(std::move(switch_statement));
    }
    auto catch_node = std::make_unique<CatchNode>(                                                        //
        file_hash, token_slice{catch_id.value(), definition.second}, err_var, body_scope, catch_base_call //
    );
    return catch_node;
}

std::optional<GroupAssignmentNode> Parser::create_group_assignment( //
    std::shared_ptr<Scope> &scope,                                  //
    const token_slice &tokens,                                      //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs             //
) {
    token_slice tokens_mut = tokens;
    ASSERT(tokens_mut.first != tokens_mut.second);
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
            THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_hash, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        if (!scope->variables.at(it_lexme).is_mutable) {
            THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_hash, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        const std::shared_ptr<Type> &expected_type = scope->variables.at(it_lexme).type;
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
    if (rhs.has_value()) {
        return GroupAssignmentNode(file_hash, tokens, assignees, rhs.value());
    }
    // The rest of the tokens now is the expression
    std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(_ctx_, scope, tokens_mut);
    if (!expr.has_value()) {
        return std::nullopt;
    }
    return GroupAssignmentNode(file_hash, tokens, assignees, expr.value());
}

std::optional<GroupAssignmentNode> Parser::create_group_assignment_shorthand( //
    std::shared_ptr<Scope> &scope,                                            //
    const token_slice &tokens,                                                //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                       //
) {
    token_slice tokens_mut = tokens;
    ASSERT(tokens_mut.first != tokens_mut.second);
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
    std::vector<token_list::iterator> assignee_iterators;
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
            THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_hash, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        if (!scope->variables.at(it_lexme).is_mutable) {
            THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_hash, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        const std::shared_ptr<Type> &expected_type = scope->variables.at(it_lexme).type;
        assignees.emplace_back(expected_type, it_lexme);
        assignee_iterators.emplace_back(it);
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
            UNREACHABLE();
            break;
    }
    tokens_mut.first++;

    // The rest of the tokens now is the expression
    std::optional<std::unique_ptr<ExpressionNode>> expr = std::nullopt;
    ASTNode::PosTriple rhs_pos;
    if (rhs.has_value()) {
        expr = std::move(rhs.value());
        rhs_pos = {
            .line = rhs.value()->line,
            .column = rhs.value()->column,
            .length = rhs.value()->length,
        };
    } else {
        expr = create_expression(_ctx_, scope, tokens_mut);
        rhs_pos = get_pos_triple(tokens_mut);
    }
    if (!expr.has_value()) {
        return std::nullopt;
    }

    // The expression now is the group of all assignees within a binop with the rhs expr
    std::vector<std::unique_ptr<ExpressionNode>> lhs_expressions;
    for (size_t i = 0; i < assignees.size(); i++) {
        const auto &[type, name] = assignees.at(i);
        const auto &it = assignee_iterators.at(i);
        lhs_expressions.emplace_back(                                                                             //
            std::make_unique<VariableNode>(file_hash, get_pos_triple(token_slice{it, it + 1}), name, type, false) //
        );
    }
    const auto &lhs_pos = get_pos_triple(token_slice{tokens.first, tokens_mut.first - 1});
    std::unique_ptr<ExpressionNode> lhs_expr = std::make_unique<GroupExpressionNode>(file_hash, lhs_pos, lhs_expressions);
    if (!check_castability(lhs_expr, expr.value())) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // The "real" expression of the assignment is a binop between the lhs and the "real" expression
    expr = std::make_unique<BinaryOpNode>( //
        file_hash,                         //
        rhs_pos,                           //
        operation,                         //
        lhs_expr,                          //
        expr.value(),                      //
        lhs_expr->type,                    //
        true                               //
    );

    return GroupAssignmentNode(file_hash, tokens, assignees, expr.value());
}

std::optional<AssignmentNode> Parser::create_assignment( //
    std::shared_ptr<Scope> &scope,                       //
    const token_slice &tokens,                           //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs  //
) {
    PROFILE_CUMULATIVE("Parser::create_assignment");
    token_list toks = clone_from_slice(tokens);
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->token == TOK_IDENTIFIER) {
            const std::string it_lexme(it->lexme);
            if (std::next(it)->token == TOK_EQUAL && ((it + 2) != tokens.second || rhs.has_value())) {
                if (scope->variables.find(it_lexme) == scope->variables.end()) {
                    THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_hash, it->line, it->column, it_lexme);
                    return std::nullopt;
                }
                if (!scope->variables.at(it_lexme).is_mutable) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_hash, it->line, it->column, it_lexme);
                    return std::nullopt;
                }
                const std::shared_ptr<Type> &expected_type = scope->variables.at(it_lexme).type;
                if (rhs.has_value()) {
                    if (!rhs.value()->type->equals(expected_type)) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    return AssignmentNode(file_hash, tokens, expected_type, it_lexme, rhs.value());
                }
                // Parse the expression with the expected type passed into it
                token_slice expression_tokens = {it + 2, tokens.second};
                std::optional<std::unique_ptr<ExpressionNode>> expression =
                    create_expression(_ctx_, scope, expression_tokens, expected_type);
                if (!expression.has_value()) {
                    return std::nullopt;
                }
                return AssignmentNode(file_hash, tokens, expected_type, it_lexme, expression.value());
            } else {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<AssignmentNode> Parser::create_assignment_shorthand( //
    std::shared_ptr<Scope> &scope,                                 //
    const token_slice &tokens,                                     //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs            //
) {
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->token != TOK_IDENTIFIER) {
            continue;
        }
        const std::string it_lexme(it->lexme);
        if (!Matcher::token_match((it + 1)->token, Matcher::assignment_shorthand_operator) || (it + 2) == tokens.second) {
            return std::nullopt;
        }
        if (scope->variables.find(it_lexme) == scope->variables.end()) {
            THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_hash, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        if (!scope->variables.at(it_lexme).is_mutable) {
            THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_hash, it->line, it->column, it_lexme);
            return std::nullopt;
        }
        const std::shared_ptr<Type> &expected_type = scope->variables.at(it_lexme).type;
        // Parse the expression with the expected type passed into it
        token_slice expression_tokens = {it + 2, tokens.second};
        std::optional<std::unique_ptr<ExpressionNode>> expression;
        ASTNode::PosTriple rhs_pos;
        if (rhs.has_value()) {
            expression = std::move(rhs.value());
            rhs_pos = {
                .line = rhs.value()->line,
                .column = rhs.value()->column,
                .length = rhs.value()->length,
            };
        } else {
            expression = create_expression(_ctx_, scope, expression_tokens, expected_type);
            rhs_pos = get_pos_triple(expression_tokens);
        }
        if (!expression.has_value()) {
            return std::nullopt;
        }
        Token op;
        switch (std::next(it)->token) {
            default:
                // It should never come here
                UNREACHABLE();
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
        std::unique_ptr<ExpressionNode> var_node = std::make_unique<VariableNode>(             //
            file_hash, get_pos_triple(token_slice{it, it + 1}), it_lexme, expected_type, false //
        );
        std::unique_ptr<ExpressionNode> bin_op = std::make_unique<BinaryOpNode>(      //
            file_hash, rhs_pos, op, var_node, expression.value(), expected_type, true //
        );
        return AssignmentNode(file_hash, tokens, expected_type, it_lexme, bin_op, true);
    }
    return std::nullopt;
}

std::optional<GroupDeclarationNode> Parser::create_group_declaration( //
    std::shared_ptr<Scope> &scope,                                    //
    const unsigned int scope_segment,                                 //
    const token_slice &tokens,                                        //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs               //
) {
    token_slice tokens_mut = tokens;
    std::optional<GroupDeclarationNode> declaration = std::nullopt;
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> variables;
    std::vector<std::pair<unsigned int, unsigned int>> var_locations;

    std::optional<uint2> lhs_range = Matcher::get_next_match_range(tokens_mut, Matcher::until_colon_equal);
    if (!lhs_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    token_slice lhs_tokens = {tokens_mut.first + lhs_range.value().first, tokens_mut.first + lhs_range.value().second};
    tokens_mut.first = lhs_tokens.second;

    // The last token now should be the COLON_EQUAL
    ASSERT(std::prev(lhs_tokens.second)->token == TOK_COLON_EQUAL);
    lhs_tokens.second--;
    remove_surrounding_paren(lhs_tokens);
    while (lhs_tokens.first != lhs_tokens.second) {
        std::optional<uint2> var_range = Matcher::get_next_match_range(lhs_tokens, Matcher::until_comma);
        if (!var_range.has_value()) {
            // The whole lhs tokens is the last variable
            ASSERT(std::prev(lhs_tokens.second)->token == TOK_IDENTIFIER || std::prev(lhs_tokens.second)->token == TOK_UNDERSCORE);
            auto name_it = std::prev(lhs_tokens.second);
            variables.emplace_back(nullptr, name_it->token == TOK_UNDERSCORE ? "" : name_it->lexme);
            var_locations.emplace_back(name_it->line, name_it->column);
            break;
        } else {
            token_slice var_tokens = {lhs_tokens.first + var_range.value().first, lhs_tokens.first + var_range.value().second};
            lhs_tokens.first = var_tokens.second;
            // The last token now should be the comma
            ASSERT(std::prev(var_tokens.second)->token == TOK_COMMA);
            var_tokens.second--;
            // The last element is the variable name now
            ASSERT(std::prev(var_tokens.second)->token == TOK_IDENTIFIER || std::prev(var_tokens.second)->token == TOK_UNDERSCORE);
            auto name_it = std::prev(var_tokens.second);
            variables.emplace_back(nullptr, name_it->token == TOK_UNDERSCORE ? "" : name_it->lexme);
            var_locations.emplace_back(name_it->line, name_it->column);
        }
    }
    // Now parse the expression (rhs)
    std::optional<std::unique_ptr<ExpressionNode>> expression = std::nullopt;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
    } else {
        expression = create_expression(_ctx_, scope, tokens_mut);
    }
    if (!expression.has_value()) {
        return std::nullopt;
    }
    switch (expression.value()->type->get_variation()) {
        default:
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return std::nullopt;
        case Type::Variation::GROUP: {
            const auto *group_type = expression.value()->type->as<GroupType>();
            const std::vector<std::shared_ptr<Type>> &types = group_type->types;
            ASSERT(variables.size() == types.size());
            for (unsigned int i = 0; i < variables.size(); i++) {
                variables.at(i).first = types.at(i);
                if (variables.at(i).second.empty()) {
                    // Skip discarded "variables" in group declarations
                    continue;
                }
                if (!scope->add_variable(variables.at(i).second,
                        Scope::Variable{
                            .type = types.at(i),
                            .scope_id = scope->scope_id,
                            .scope_segment = scope_segment,
                            .is_mutable = true,
                            .is_persistent = false,
                            .is_fn_param = false,
                            .is_reference = false,
                            .return_scope_ids = {},
                            .is_pseudo_variable = false,
                            .file_hash = file_hash,
                            .line = var_locations.at(i).first,
                            .column = var_locations.at(i).second,
                        }) //
                ) {
                    // Variable shadowing
                    THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_hash, lhs_tokens.first->line, lhs_tokens.first->column,
                        variables.at(i).second);
                    return std::nullopt;
                }
            }
            return GroupDeclarationNode(file_hash, tokens, variables, expression.value());
        }
        case Type::Variation::VECTOR: {
            const auto *vector_type = expression.value()->type->as<VectorType>();
            for (unsigned int i = 0; i < variables.size(); i++) {
                variables.at(i).first = vector_type->base_type;
                if (variables.at(i).second.empty()) {
                    // Skip discarded "variables" in group declarations
                    continue;
                }
                if (!scope->add_variable(variables.at(i).second,
                        Scope::Variable{
                            .type = vector_type->base_type,
                            .scope_id = scope->scope_id,
                            .scope_segment = scope_segment,
                            .is_mutable = true,
                            .is_persistent = false,
                            .is_fn_param = false,
                            .is_reference = false,
                            .return_scope_ids = {},
                            .is_pseudo_variable = false,
                            .file_hash = file_hash,
                            .line = var_locations.at(i).first,
                            .column = var_locations.at(i).second,
                        }) //
                ) {
                    THROW_ERR(                                                                   //
                        ErrVarRedefinition, ERR_PARSING, file_hash,                              //
                        lhs_tokens.first->line, lhs_tokens.first->column, variables.at(i).second //
                    );
                    return std::nullopt;
                }
            }
            std::string group_type_str = "(";
            for (unsigned int i = 0; i < vector_type->width; i++) {
                if (i > 0) {
                    group_type_str += ", ";
                }
                group_type_str += vector_type->base_type->to_string();
            }
            group_type_str += ")";
            std::optional<std::shared_ptr<Type>> expr_group_type = file_node_ptr->file_namespace->get_type_from_str(group_type_str);
            if (!expr_group_type.has_value()) {
                std::vector<std::shared_ptr<Type>> group_types;
                for (unsigned int i = 0; i < vector_type->width; i++) {
                    group_types.emplace_back(vector_type->base_type);
                }
                expr_group_type = std::make_shared<GroupType>(group_types);
                if (!file_node_ptr->file_namespace->add_type(expr_group_type.value())) {
                    expr_group_type = file_node_ptr->file_namespace->get_type_from_str(expr_group_type.value()->to_string()).value();
                }
            }
            if (!check_castability(expr_group_type.value(), expression.value())) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return GroupDeclarationNode(file_hash, tokens, variables, expression.value());
        }
        case Type::Variation::TUPLE: {
            const auto *tuple_type = expression.value()->type->as<TupleType>();
            if (variables.size() != tuple_type->types.size()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            for (unsigned int i = 0; i < variables.size(); i++) {
                variables.at(i).first = tuple_type->types[i];
                if (variables.at(i).second.empty()) {
                    // Skip discarded "variables" in group declarations
                    continue;
                }
                if (!scope->add_variable(variables.at(i).second,
                        Scope::Variable{
                            .type = tuple_type->types[i],
                            .scope_id = scope->scope_id,
                            .scope_segment = scope_segment,
                            .is_mutable = true,
                            .is_persistent = false,
                            .is_fn_param = false,
                            .is_reference = false,
                            .return_scope_ids = {},
                            .is_pseudo_variable = false,
                            .file_hash = file_hash,
                            .line = var_locations.at(i).first,
                            .column = var_locations.at(i).second,
                        }) //
                ) {
                    THROW_ERR(                                                                   //
                        ErrVarRedefinition, ERR_PARSING, file_hash,                              //
                        lhs_tokens.first->line, lhs_tokens.first->column, variables.at(i).second //
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
            std::optional<std::shared_ptr<Type>> expr_group_type = file_node_ptr->file_namespace->get_type_from_str(group_type_str);
            if (!expr_group_type.has_value()) {
                std::vector<std::shared_ptr<Type>> group_types;
                for (unsigned int i = 0; i < tuple_type->types.size(); i++) {
                    group_types.emplace_back(tuple_type->types[i]);
                }
                expr_group_type = std::make_shared<GroupType>(group_types);
                if (!file_node_ptr->file_namespace->add_type(expr_group_type.value())) {
                    expr_group_type = file_node_ptr->file_namespace->get_type_from_str(expr_group_type.value()->to_string()).value();
                }
            }
            if (!check_castability(expr_group_type.value(), expression.value())) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return GroupDeclarationNode(file_hash, tokens, variables, expression.value());
        }
    }
}

std::optional<DeclarationNode> Parser::create_declaration( //
    std::shared_ptr<Scope> &scope,                         //
    const unsigned int scope_segment,                      //
    const token_slice &tokens,                             //
    const bool is_inferred,                                //
    const bool has_rhs,                                    //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs    //
) {
    token_slice tokens_mut = tokens;
    ASSERT(!(is_inferred && !has_rhs));

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
    // Check if a persistent variable is declared
    const bool is_persistent = lhs_tokens.first->token == TOK_PERSISTENT;
    if (is_persistent) {
        if (!is_mutable) {
            // Persistent locals always must be mutable
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (std::holds_alternative<TestNode *>(scope->function)) {
            // Persistent locals are not allowed within a test function
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        lhs_tokens.first++;
    }

    // Get the type if it is not inferred and get the name of the declaration
    std::shared_ptr<Type> declared_type;
    std::string name;
    if (!is_inferred) {
        if (lhs_tokens.first->token != TOK_TYPE) {
            ASSERT((lhs_tokens.second - 1) != lhs_tokens.first);
            ASSERT((lhs_tokens.second - 2) != lhs_tokens.first);
            THROW_ERR(ErrUnknownType, ERR_PARSING, file_hash, token_slice{lhs_tokens.first, lhs_tokens.second - 2});
            return std::nullopt;
        }
        declared_type = lhs_tokens.first->type;
        ASSERT(std::next(lhs_tokens.first)->token == TOK_IDENTIFIER);
        name = std::next(lhs_tokens.first)->lexme;
    } else {
        ASSERT(lhs_tokens.first->token == TOK_IDENTIFIER);
        name = lhs_tokens.first->lexme;
    }

    // Case 1: Declaration without RHS
    if (!has_rhs) {
        ASSERT(!is_inferred);
        ASSERT(declared_type != nullptr);
        if (is_persistent) {
            // Persistent locals require an initializer
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (declared_type->is_freeable()) {
            // Freeable types are not allowed to not have an initializer
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }

        std::optional<std::unique_ptr<ExpressionNode>> expr = std::nullopt;
        if (!scope->add_variable(name,
                Scope::Variable{
                    .type = declared_type,
                    .scope_id = scope->scope_id,
                    .scope_segment = scope_segment,
                    .is_mutable = is_mutable,
                    .is_persistent = is_persistent,
                    .is_fn_param = false,
                    .is_reference = false,
                    .return_scope_ids = {},
                    .is_pseudo_variable = false,
                    .file_hash = file_hash,
                    .line = std::next(lhs_tokens.first)->line,
                    .column = std::next(lhs_tokens.first)->column,
                }) //
        ) {
            THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_hash, std::next(lhs_tokens.first)->line, std::next(lhs_tokens.first)->column,
                name);
            return std::nullopt;
        }
        return DeclarationNode(file_hash, tokens, declared_type, name, is_persistent, expr);
    }

    // Case 2 & 3: Declaration with RHS - create expression if not provided
    if (!rhs.has_value()) {
        rhs = create_expression(_ctx_, scope, tokens_mut);
        if (!rhs.has_value()) {
            return std::nullopt;
        }
    }

    // Determine final type and handle conversions
    std::shared_ptr<Type> final_type = declared_type;
    if (is_inferred) {
        // Resolve literals to default types for inferred declarations
        resolve_comptime_type_of_expr(rhs.value(), std::nullopt);
        final_type = rhs.value()->type;
        if (rhs.value()->type->get_variation() == Type::Variation::GROUP) {
            const ASTNode::PosTriple rhs_pos{
                .line = rhs.value()->line,
                .column = rhs.value()->column,
                .length = rhs.value()->length,
            };
            const GroupType *group_type = rhs.value()->type->as<GroupType>();
            std::optional<std::shared_ptr<Type>> homogeneous_type = group_type->get_homogeneous_type();
            const size_t group_width = group_type->types.size();
            const bool group_2_3_4_wide = group_width == 2 || group_width == 3 || group_width == 4;
            const bool group_8_wide = group_width == 8;
            if (homogeneous_type.has_value() && (group_2_3_4_wide || group_8_wide)) {
                // It may be able to turn into a vector type
                const auto &type = homogeneous_type.value();
                if (type->get_variation() == Type::Variation::PRIMITIVE) {
                    const std::string &primitive_name = type->as<PrimitiveType>()->type_name;
                    // It is not a vector type if:
                    //   - The base type is a string *or*
                    //   - (The base type is a bool *and* the group is not 8 elements large) *or*
                    //   - (The base type is 64 bit *and* the group is 8 elements large)
                    const bool is_str = primitive_name == "str";
                    const bool is_invalid_bool8 = primitive_name == "bool" && !group_8_wide;
                    const bool is_invalid_64bit = primitive_name.substr(primitive_name.size() - 2) == "64" && group_8_wide;
                    const bool is_not_vector = is_str || is_invalid_bool8 || is_invalid_64bit;
                    if (!is_not_vector) {
                        // The rhs expression needs to be wrapped in a `TypeCastNode` since just changing the type of the group
                        // expression does not properly work. We instead need to cast the group to a vector
                        rhs.value()->type = final_type;
                        const std::string &vector_type_string = primitive_name + "x" + std::to_string(group_width);
                        const auto vec_ty = file_node_ptr->file_namespace->get_type_from_str(vector_type_string).value();
                        rhs = std::make_unique<TypeCastNode>(file_hash, rhs_pos, vec_ty, rhs.value());
                        final_type = vec_ty;
                    }
                }
            }
            if (!homogeneous_type.has_value()) {
                // It was not able to turn into a vector type, so we turn it into a tuple
                final_type = std::make_shared<TupleType>(group_type->types);
                if (!file_node_ptr->file_namespace->add_type(final_type)) {
                    final_type = file_node_ptr->file_namespace->get_type_from_str(final_type->to_string()).value();
                }
                rhs = std::make_unique<TypeCastNode>(file_hash, rhs_pos, final_type, rhs.value());
            }
            final_type = rhs.value()->type;
        }
    }
    if (!check_castability(final_type, rhs.value())) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens_mut, final_type, rhs.value()->type);
        return std::nullopt;
    }

    // Special handling for switch expressions
    if (rhs.value()->get_variation() == ExpressionNode::Variation::SWITCH_EXPRESSION) {
        auto *switch_expr = rhs.value()->as<SwitchExpression>();
        for (auto &branch : switch_expr->branches) {
            if (!check_castability(final_type, branch.expr)) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
        }
    }

    // Add variable to scope
    if (!scope->add_variable(name,
            Scope::Variable{
                .type = final_type,
                .scope_id = scope->scope_id,
                .scope_segment = scope_segment,
                .is_mutable = is_mutable,
                .is_persistent = is_persistent,
                .is_fn_param = false,
                .is_reference = false,
                .return_scope_ids = {},
                .is_pseudo_variable = false,
                .file_hash = file_hash,
                .line = is_inferred ? lhs_tokens.first->line : std::next(lhs_tokens.first)->line,
                .column = is_inferred ? lhs_tokens.first->column : std::next(lhs_tokens.first)->column,
            }) //
    ) {
        THROW_ERR(ErrVarRedefinition, ERR_PARSING, file_hash, is_inferred ? lhs_tokens.first->line : std::next(lhs_tokens.first)->line,
            is_inferred ? lhs_tokens.first->column : std::next(lhs_tokens.first)->column, name);
        return std::nullopt;
    }

    return DeclarationNode(file_hash, tokens, final_type, name, is_persistent, rhs);
}

std::optional<UnaryOpStatement> Parser::create_unary_op_statement(std::shared_ptr<Scope> &scope, const token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::create_unary_op_statement");
    auto unary_op_base = create_unary_op_base(_ctx_, scope, tokens);
    if (!unary_op_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    return UnaryOpStatement(             //
        file_hash, tokens,               //
        unary_op_base.value().operation, //
        unary_op_base.value().base_expr, //
        unary_op_base.value().is_left    //
    );
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_data_field_assignment( //
    std::shared_ptr<Scope> &scope,                                                  //
    const token_slice &tokens,                                                      //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                             //
) {
    PROFILE_CUMULATIVE("Parser::create_data_field_assignment");
    // Everything up to the equals sign is the lhs of the assignment
    token_slice tokens_mut = tokens;
    token_slice lhs_tokens = {tokens.first, tokens.first};
    while (lhs_tokens.second->token != TOK_EQUAL) {
        lhs_tokens.second++;
        tokens_mut.first++;
    }
    auto field_access_base = create_field_access_base(_ctx_, scope, lhs_tokens);
    if (!field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now the equal sign should follow, we will delete that one too
    ASSERT(tokens_mut.first->token == TOK_EQUAL);
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
    } else {
        expression = create_expression(_ctx_, scope, tokens_mut);
    }
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    std::unique_ptr<ExpressionNode> &base_expr = field_access_base.value().base_expr;
    // If the field base is a type expression, check if it's a shared data, as then it's a global variable assignment, not a field
    // assignment
    if (base_expr->get_variation() == ExpressionNode::Variation::TYPE                //
        && base_expr->as<TypeNode>()->type->get_variation() == Type::Variation::DATA //
        && base_expr->as<TypeNode>()->type->as<DataType>()->data_node->is_shared     //
    ) {
        // Shared data assignment
        const auto *data_type = tokens.first->type->as<DataType>();
        const auto &data_node = data_type->data_node;
        const std::string field_name((tokens.first + 2)->lexme);
        const auto &fields = data_node->fields;
        auto field = fields.begin();
        for (; field != fields.end(); ++field) {
            if (field->name == field_name)
                break;
        }
        if (field == fields.end()) {
            THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, tokens.first->type, std::nullopt);
            return std::nullopt;
        }
        const std::string mangled_name = data_node->file_hash.to_string() + ".shared." + data_node->name + "." + field_name;
        if (scope->variables.find(mangled_name) == scope->variables.end()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const auto &var = scope->variables.at(mangled_name);
        // A global variable cannot be defined as const
        ASSERT(var.is_mutable);
        const token_slice rhs_tokens = {tokens.first + 4, tokens.second};
        if (!check_castability(var.type, expression.value())) {
            THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, rhs_tokens, var.type, expression.value()->type);
            return std::nullopt;
        }
        return std::make_unique<AssignmentNode>(file_hash, tokens, var.type, mangled_name, expression.value());
    }

    // The data field base expression should be a variable expression
    if (base_expr->is_const) {
        THROW_ERR(ErrExprMutatingConst, ERR_PARSING, file_hash, tokens);
        return std::nullopt;
    }
    const auto &field_type = field_access_base.value().field_type;
    if (!check_castability(field_type, expression.value())) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens_mut, field_type, expression.value()->type);
        return std::nullopt;
    }

    return std::make_unique<DataFieldAssignmentNode>( //
        file_hash, tokens,                            //
        base_expr,                                    //
        field_access_base.value().field_name,         //
        field_access_base.value().field_id,           //
        field_type,                                   //
        expression.value()                            //
    );
}

std::optional<DataFieldAssignmentNode> Parser::create_data_field_assignment_shorthand( //
    std::shared_ptr<Scope> &scope,                                                     //
    const token_slice &tokens,                                                         //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                                //
) {
    PROFILE_CUMULATIVE("Parser::create_data_field_assignment");
    // Everything up to the equals sign is the lhs of the assignment
    token_slice tokens_mut = tokens;
    token_slice lhs_tokens = {tokens.first, tokens.first};
    while (!Matcher::token_match(lhs_tokens.second->token, Matcher::assignment_shorthand_operator)) {
        lhs_tokens.second++;
        tokens_mut.first++;
    }
    auto field_access_base = create_field_access_base(_ctx_, scope, lhs_tokens);
    if (!field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    auto &base_expr = field_access_base.value().base_expr;
    if (base_expr->is_const) {
        THROW_ERR(ErrExprMutatingConst, ERR_PARSING, file_hash, tokens);
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
            UNREACHABLE();
            break;
    }
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression;
    ASTNode::PosTriple rhs_pos;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
        rhs_pos = {
            .line = rhs.value()->line,
            .column = rhs.value()->column,
            .length = rhs.value()->length,
        };
    } else {
        expression = create_expression(_ctx_, scope, tokens_mut);
        rhs_pos = get_pos_triple(tokens_mut);
    }
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Create the lhs of the binary op of the right side, which is the data field access
    const auto &field_name = field_access_base.value().field_name;
    const auto field_id = field_access_base.value().field_id;
    const auto &field_type = field_access_base.value().field_type;
    auto base_expr_clone = base_expr->clone(scope->scope_id);
    std::unique_ptr<ExpressionNode> binop_lhs = std::make_unique<DataAccessNode>( //
        file_hash,                                                                //
        get_pos_triple(token_slice{tokens.first, tokens_mut.first}),              //
        base_expr_clone,                                                          //
        field_name,                                                               //
        field_id,                                                                 //
        field_type                                                                //
    );

    if (!check_castability(field_type, expression.value())) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens_mut, field_type, expression.value()->type);
        return std::nullopt;
    }
    expression = std::make_unique<BinaryOpNode>( //
        file_hash,                               //
        rhs_pos,                                 //
        operation,                               //
        binop_lhs,                               //
        expression.value(),                      //
        field_type,                              //
        true                                     //
    );
    return DataFieldAssignmentNode(file_hash, tokens, base_expr, field_name, field_id, field_type, expression.value());
}

std::optional<GroupedDataFieldAssignmentNode> Parser::create_grouped_data_field_assignment( //
    std::shared_ptr<Scope> &scope,                                                          //
    const token_slice &tokens,                                                              //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                                     //
) {
    PROFILE_CUMULATIVE("Parser::create_grouped_data_field_assignment");
    token_slice tokens_mut = tokens;
    token_slice lhs_tokens = {tokens.first, tokens.first};
    while (lhs_tokens.second->token != TOK_EQUAL) {
        lhs_tokens.second++;
        tokens_mut.first++;
    }
    auto grouped_field_access_base = create_grouped_access_base(_ctx_, scope, lhs_tokens);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    auto &base_expr = grouped_field_access_base.value().base_expr;
    if (base_expr->is_const) {
        THROW_ERR(ErrExprMutatingConst, ERR_PARSING, file_hash, lhs_tokens);
        return std::nullopt;
    }

    // Now the equal sign should follow, we will delete that one too
    ASSERT(tokens_mut.first->token == TOK_EQUAL);
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
    } else {
        expression = create_expression(_ctx_, scope, tokens_mut);
    }
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const auto &field_names = grouped_field_access_base.value().field_names;
    const auto &field_ids = grouped_field_access_base.value().field_ids;
    const auto &field_types = grouped_field_access_base.value().field_types;
    const std::shared_ptr<Type> group_type = std::make_shared<GroupType>(field_types);
    if (!check_castability(group_type, expression.value())) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens_mut, group_type, expression.value()->type);
        return std::nullopt;
    }
    return GroupedDataFieldAssignmentNode(file_hash, tokens, base_expr, field_names, field_ids, field_types, expression.value());
}

std::optional<GroupedDataFieldAssignmentNode> Parser::create_grouped_data_field_assignment_shorthand( //
    std::shared_ptr<Scope> &scope,                                                                    //
    const token_slice &tokens,                                                                        //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                                               //
) {
    PROFILE_CUMULATIVE("Parser::create_grouped_data_field_assignment_shorthand");
    token_slice tokens_mut = tokens;
    token_slice lhs_tokens = {tokens.first, tokens.first};
    while (!Matcher::token_match(lhs_tokens.second->token, Matcher::assignment_shorthand_operator)) {
        lhs_tokens.second++;
        tokens_mut.first++;
    }
    auto grouped_field_access_base = create_grouped_access_base(_ctx_, scope, lhs_tokens);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    auto &base_expr = grouped_field_access_base.value().base_expr;
    if (base_expr->is_const) {
        THROW_ERR(ErrExprMutatingConst, ERR_PARSING, file_hash, lhs_tokens);
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
            UNREACHABLE();
            break;
    }
    tokens_mut.first++;

    // The rest of the tokens is the expression to parse
    std::optional<std::unique_ptr<ExpressionNode>> expression;
    ASTNode::PosTriple rhs_pos;
    if (rhs.has_value()) {
        expression = std::move(rhs.value());
        rhs_pos = {
            .line = rhs.value()->line,
            .column = rhs.value()->column,
            .length = rhs.value()->length,
        };
    } else {
        expression = create_expression(_ctx_, scope, tokens_mut);
        rhs_pos = get_pos_triple(tokens_mut);
    }
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Create the lhs of the binary op of the right side, which is the grouped data field access
    const auto &field_names = grouped_field_access_base.value().field_names;
    const auto &field_ids = grouped_field_access_base.value().field_ids;
    const auto &field_types = grouped_field_access_base.value().field_types;
    auto base_expr_clone = base_expr->clone(scope->scope_id);
    std::unique_ptr<ExpressionNode> binop_lhs = std::make_unique<GroupedDataAccessNode>( //
        file_hash,                                                                       //
        get_pos_triple(token_slice{tokens.first, tokens_mut.first}),                     //
        base_expr_clone,                                                                 //
        field_names,                                                                     //
        field_ids,                                                                       //
        field_types                                                                      //
    );

    // The expression already is the rhs of the binop, so now we only need to check whether the types of the expression matches the types of
    // the assignment
    const std::shared_ptr<Type> group_type = std::make_shared<GroupType>(field_types);
    if (!check_castability(group_type, expression.value())) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens_mut, group_type, expression.value()->type);
        return std::nullopt;
    }
    expression = std::make_unique<BinaryOpNode>( //
        file_hash,                               //
        rhs_pos,                                 //
        operation,                               //
        binop_lhs,                               //
        expression.value(),                      //
        binop_lhs->type,                         //
        true                                     //
    );
    return GroupedDataFieldAssignmentNode(file_hash, tokens, base_expr, field_names, field_ids, field_types, expression.value());
}

std::optional<ArrayAssignmentNode> Parser::create_array_assignment( //
    std::shared_ptr<Scope> &scope,                                  //
    const token_slice &tokens,                                      //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs             //
) {
    PROFILE_CUMULATIVE("Parser::create_array_assignment");
    token_slice lhs_tokens = {tokens.first, tokens.first};
    // Find the = operator
    while (lhs_tokens.second != tokens.second) {
        if (lhs_tokens.second->token == TOK_EQUAL) {
            break;
        }
        lhs_tokens.second++;
    }
    if (lhs_tokens.second == tokens.second) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    ASSERT(lhs_tokens.second->token == TOK_EQUAL);

    // Create the access base from the lhs tokens
    auto access_base = create_array_access_base(_ctx_, scope, lhs_tokens);
    if (!access_base.has_value()) {
        return std::nullopt;
    }

    // If no rhs was provided we need to parse it ourselves, otherwise we can use the provided rhs expression
    if (!rhs.has_value()) {
        const token_slice rhs_tokens = {lhs_tokens.second + 1, tokens.second};
        rhs = create_expression(_ctx_, scope, rhs_tokens, access_base.value().result_type);
        if (!rhs.has_value()) {
            return std::nullopt;
        }
    }
    return ArrayAssignmentNode(file_hash, tokens, access_base.value().base_expr, access_base.value().indexing_exprs, rhs.value());
}

std::optional<ArrayAssignmentNode> Parser::create_array_assignment_shorthand( //
    std::shared_ptr<Scope> &scope,                                            //
    const token_slice &tokens,                                                //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                       //
) {
    PROFILE_CUMULATIVE("Parser::create_array_assignment_shorthand");
    token_slice tokens_mut = tokens;
    token_slice lhs_tokens = {tokens.first, tokens.first};
    while (!Matcher::token_match(lhs_tokens.second->token, Matcher::assignment_shorthand_operator)) {
        lhs_tokens.second++;
        tokens_mut.first++;
    }
    Token operation = TOK_EOF;
    switch (lhs_tokens.second->token) {
        default:
            // Should not happen, at least one assignment shorthand operator should be present because otherwise the matcher would not have
            // matched this statement as an array assignment shorthand
            UNREACHABLE();
            return std::nullopt;
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
    }

    // Create the access base from the lhs tokens
    auto access_base = create_array_access_base(_ctx_, scope, lhs_tokens);
    if (!access_base.has_value()) {
        return std::nullopt;
    }

    // If no rhs was provided we need to parse it ourselves, otherwise we can use the provided rhs expression
    ASTNode::PosTriple rhs_pos;
    if (!rhs.has_value()) {
        const token_slice rhs_tokens = {lhs_tokens.second + 1, tokens.second};
        rhs = create_expression(_ctx_, scope, rhs_tokens, access_base.value().result_type);
        if (!rhs.has_value()) {
            return std::nullopt;
        }
        rhs_pos = get_pos_triple(rhs_tokens);
    } else {
        rhs_pos = {
            .line = rhs.value()->line,
            .column = rhs.value()->column,
            .length = rhs.value()->length,
        };
    }

    // Calculate the new dimensionality of the result based on the number of range expressions in the indexing expressions
    uint32_t dimensionality = 0;
    for (const auto &expr : access_base.value().indexing_exprs) {
        if (expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
            dimensionality++;
        }
    }
    const auto &base_type = access_base.value().base_expr->type->as<ArrayType>()->type;
    std::shared_ptr<Type> result_type = nullptr;
    if (dimensionality > 0) {
        // TODO: Known Sizes
        result_type = std::make_shared<ArrayType>(dimensionality, base_type, std::nullopt);
        if (!file_node_ptr->file_namespace->add_type(result_type)) {
            result_type = file_node_ptr->file_namespace->get_type_from_str(result_type->to_string()).value();
        }
    } else {
        result_type = base_type;
    }

    // Since it's an array assignment shorthand we need to clone the base expression and put it into a binary op together with the rhs
    // expression and use that as our new rhs expression
    auto base_expr_clone = access_base.value().base_expr->clone(scope->scope_id);
    std::vector<std::unique_ptr<ExpressionNode>> indexing_exprs_clone;
    for (const auto &expr : access_base.value().indexing_exprs) {
        indexing_exprs_clone.emplace_back(expr->clone(scope->scope_id));
    }
    std::unique_ptr<ExpressionNode> arr_access = std::make_unique<ArrayAccessNode>(                                                //
        file_hash, get_pos_triple(token_slice{tokens.first, tokens_mut.first}), base_expr_clone, result_type, indexing_exprs_clone //
    );
    rhs = std::make_unique<BinaryOpNode>(                                              //
        file_hash, rhs_pos, operation, arr_access, rhs.value(), arr_access->type, true //
    );
    return ArrayAssignmentNode(file_hash, tokens, access_base.value().base_expr, access_base.value().indexing_exprs, rhs.value());
}

std::optional<GroupedArrayAssignmentNode> Parser::create_grouped_array_assignment( //
    std::shared_ptr<Scope> &scope,                                                 //
    const token_slice &tokens,                                                     //
    std::optional<std::unique_ptr<ExpressionNode>> &rhs                            //
) {
    PROFILE_CUMULATIVE("Parser::create_grouped_array_assignment");
    token_slice lhs_tokens = {tokens.first, tokens.first};
    // Find the = operator
    while (lhs_tokens.second != tokens.second) {
        if (lhs_tokens.second->token == TOK_EQUAL) {
            break;
        }
        lhs_tokens.second++;
    }
    if (lhs_tokens.second == tokens.second) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    ASSERT(lhs_tokens.second->token == TOK_EQUAL);

    // Create the access base from the lhs tokens
    auto access_base = create_array_access_base(_ctx_, scope, lhs_tokens);
    if (!access_base.has_value()) {
        return std::nullopt;
    }

    // If no rhs was provided we need to parse it ourselves, otherwise we can use the provided rhs expression
    if (!rhs.has_value()) {
        const token_slice rhs_tokens = {lhs_tokens.second + 1, tokens.second};
        rhs = create_expression(_ctx_, scope, rhs_tokens, access_base.value().result_type);
        if (!rhs.has_value()) {
            return std::nullopt;
        }
    }
    return GroupedArrayAssignmentNode(file_hash, tokens, access_base.value().base_expr, access_base.value().indexing_exprs, rhs.value());
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_statement( //
    std::shared_ptr<Scope> &scope,                                      //
    const unsigned int scope_segment,                                   //
    const token_slice &tokens,                                          //
    std::optional<std::unique_ptr<ExpressionNode>> rhs                  //
) {
    std::optional<std::unique_ptr<StatementNode>> statement_node = std::nullopt;
    [[maybe_unused]] token_list tok_list = clone_from_slice(tokens);

    if (Matcher::tokens_contain(tokens, Matcher::group_declaration_inferred)) {
        std::optional<GroupDeclarationNode> group_decl = create_group_declaration(scope, scope_segment, tokens, rhs);
        if (!group_decl.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupDeclarationNode>(std::move(group_decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::declaration_explicit)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, scope_segment, tokens, false, true, rhs);
        if (!decl.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::declaration_inferred)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, scope_segment, tokens, true, true, rhs);
        if (!decl.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_match(tokens, Matcher::declaration_without_initializer)) {
        std::optional<DeclarationNode> decl = create_declaration(scope, scope_segment, tokens, false, false, rhs);
        if (!decl.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::data_field_assignment)) {
        std::optional<std::unique_ptr<StatementNode>> assign = create_data_field_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(assign);
    } else if (Matcher::tokens_contain(tokens, Matcher::data_field_assignment_shorthand)) {
        std::optional<DataFieldAssignmentNode> assign = create_data_field_assignment_shorthand(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<DataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::grouped_data_assignment)) {
        std::optional<GroupedDataFieldAssignmentNode> assign = create_grouped_data_field_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupedDataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::grouped_data_assignment_shorthand)) {
        std::optional<GroupedDataFieldAssignmentNode> assign = create_grouped_data_field_assignment_shorthand(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupedDataFieldAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::group_assignment)) {
        std::optional<GroupAssignmentNode> assign = create_group_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::group_assignment_shorthand)) {
        std::optional<GroupAssignmentNode> assign = create_group_assignment_shorthand(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::array_assignment)) {
        std::optional<ArrayAssignmentNode> assign = create_array_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<ArrayAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::array_assignment_shorthand)) {
        std::optional<ArrayAssignmentNode> assign = create_array_assignment_shorthand(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<ArrayAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::grouped_array_assignment)) {
        std::optional<GroupedArrayAssignmentNode> assign = create_grouped_array_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<GroupedArrayAssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::grouped_array_assignment_shorthand)) {
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    } else if (Matcher::tokens_contain(tokens, Matcher::assignment)) {
        std::optional<AssignmentNode> assign = create_assignment(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<AssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::assignment_shorthand)) {
        std::optional<AssignmentNode> assign = create_assignment_shorthand(scope, tokens, rhs);
        if (!assign.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<AssignmentNode>(std::move(assign.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::discard_assignment)) {
        const token_slice rhs_tokens = {tokens.first + 2, tokens.second};
        auto rhs_expr = create_expression(_ctx_, scope, rhs_tokens);
        if (!rhs_expr.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<AssignmentNode>(file_hash, tokens, rhs_expr.value()->type, "_", rhs_expr.value());
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
    } else if (Matcher::tokens_contain(tokens, Matcher::aliased_function_call)                      //
        && Matcher::get_next_match_range(tokens, Matcher::aliased_function_call).value().first == 0 //
    ) {
        // Only check for an aliased function call if the aliased function call is at the start of this statement (to prevent it being
        // recognized in the expressions of this statement, for example a aliased function call / variant tag initializer / func component call
        // within a call)
        token_slice tokens_mut = tokens;
        if (tokens_mut.first->token == TOK_TYPE) {
            switch (tokens_mut.first->type->get_variation()) {
                default:
                    // Aliased function calls are only allowed on objects or func components, no other *type* can contain functions
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                case Type::Variation::OBJECT: {
                    const auto *object_node = tokens_mut.first->type->as<ObjectType>()->object_node;
                    if (object_node->file_hash.to_string() != file_hash.to_string()) {
                        auto *object_namespace = Resolver::get_namespace_from_hash(object_node->file_hash);
                        statement_node = create_call_statement(scope, tokens_mut, object_namespace, true);
                    } else {
                        statement_node = create_call_statement(scope, tokens_mut, std::nullopt, true);
                    }
                    break;
                }
                case Type::Variation::FUNC: {
                    const auto *func_node = tokens_mut.first->type->as<FuncType>()->func_node;
                    if (func_node->file_hash.to_string() != file_hash.to_string()) {
                        auto *func_namespace = Resolver::get_namespace_from_hash(func_node->file_hash);
                        statement_node = create_call_statement(scope, tokens_mut, func_namespace, true);
                    } else {
                        statement_node = create_call_statement(scope, tokens_mut, std::nullopt, true);
                    }
                    break;
                }
            }
        } else {
            ASSERT(tokens_mut.first->token == TOK_ALIAS && std::next(tokens_mut.first)->token == TOK_DOT);
            Namespace *alias_namespace = tokens_mut.first->alias_namespace;
            tokens_mut.first += 2;
            statement_node = create_call_statement(scope, tokens_mut, alias_namespace);
        }
    } else if (Matcher::tokens_contain(tokens, Matcher::function_call)) {
        statement_node = create_call_statement(scope, tokens, std::nullopt);
    } else if (Matcher::tokens_end_with_continuous(                                                                                  //
                   token_slice{tokens.first, std::prev(tokens.second)}, Matcher::unary_post_operator, Matcher::expression_separator) //
    ) {
        std::optional<UnaryOpStatement> unary_op = create_unary_op_statement(scope, tokens);
        if (!unary_op.has_value()) {
            return std::nullopt;
        }
        statement_node = std::make_unique<UnaryOpStatement>(std::move(unary_op.value()));
    } else if (Matcher::tokens_contain(tokens, Matcher::break_statement)) {
        statement_node = std::make_unique<BreakNode>(file_hash, tokens);
    } else if (Matcher::tokens_contain(tokens, Matcher::continue_statement)) {
        statement_node = std::make_unique<ContinueNode>(file_hash, tokens);
    } else {
        token_list toks = clone_from_slice(tokens);
        UNREACHABLE();
    }
    if (!statement_node.has_value()) {
        return std::nullopt;
    }
    statement_node.value()->file_hash = file_hash;
    statement_node.value()->line = tokens.first->line;
    statement_node.value()->column = tokens.first->column;
    statement_node.value()->length = tokens.second->column - tokens.first->column;
    return statement_node;
}

std::optional<std::unique_ptr<StatementNode>> Parser::create_scoped_statement( //
    std::shared_ptr<Scope> &scope,                                             //
    const unsigned int scope_segment,                                          //
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
            THROW_ERR(ErrMissingBody, ERR_PARSING, file_hash, (line_it - 1)->tokens);
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
            THROW_ERR(ErrStmtIfChainMissingIf, ERR_PARSING, file_hash, definition);
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

        std::optional<std::unique_ptr<IfNode>> if_node = create_if(scope, scope_segment, if_chain);
        if (!if_node.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(if_node.value());
    } else if (Matcher::tokens_contain(definition, Matcher::for_loop)) {
        std::optional<std::unique_ptr<ForLoopNode>> for_loop = create_for_loop(scope, scope_segment, definition, scoped_body.value());
        if (!for_loop.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(for_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::par_for_loop) ||
        Matcher::tokens_contain(definition, Matcher::enhanced_for_loop)) {
        std::optional<std::unique_ptr<EnhForLoopNode>> enh_for_loop = create_enh_for_loop( //
            scope, scope_segment, definition, scoped_body.value()                          //
        );
        if (!enh_for_loop.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(enh_for_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::while_loop)) {
        std::optional<std::unique_ptr<WhileNode>> while_loop = create_while_loop(scope, scope_segment, definition, scoped_body.value());
        if (!while_loop.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(while_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::do_while_loop)) {
        const auto &condition_line = line_it->tokens;
        ++line_it;
        std::optional<std::unique_ptr<DoWhileNode>> do_while_loop = create_do_while_loop( //
            scope, scope_segment, condition_line, scoped_body.value()                     //
        );
        if (!do_while_loop.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(do_while_loop.value());
    } else if (Matcher::tokens_contain(definition, Matcher::catch_statement)) {
        std::optional<std::unique_ptr<CatchNode>> catch_node = create_catch(  //
            scope, scope_segment, definition, scoped_body.value(), statements //
        );
        if (!catch_node.has_value()) {
            return std::nullopt;
        }
        statement_node = std::move(catch_node.value());
    } else if (Matcher::tokens_contain(definition, Matcher::aliased_function_call)) {
        ASSERT(definition.first->token == TOK_ALIAS && std::next(definition.first)->token == TOK_DOT);
        Namespace *alias_namespace = definition.first->alias_namespace;
        statement_node = create_call_statement(scope, {definition.first + 2, definition.second}, alias_namespace);
    } else if (Matcher::tokens_contain(definition, Matcher::switch_statement)) {
        statement_node = create_switch_statement(scope, scope_segment, definition, scoped_body.value());
    } else if (Matcher::tokens_contain(definition, Matcher::function_call)) {
        statement_node = create_call_statement(scope, definition, std::nullopt);
    } else {
        // Unknown scoped statement
        token_list toks = clone_from_slice(definition);
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    if (statement_node.value()->get_variation() != StatementNode::Variation::IF) {
        // Every scoped statement other than if gets it's end line updated. For if statements we have special-case handling directly in its
        // creation
        statement_node.value()->end_line = scoped_body.value().back().tokens.second->line;
    }

    return statement_node;
}

std::optional<std::vector<std::unique_ptr<StatementNode>>> Parser::create_body( //
    std::shared_ptr<Scope> &scope,                                              //
    const std::vector<Line> &body                                               //
) {
    std::vector<std::unique_ptr<StatementNode>> body_statements;
    unsigned int scope_segment = 0;

    for (auto line_it = body.begin(); line_it != body.end();) {
        token_slice statement_tokens = {line_it->tokens.first, line_it->tokens.second};
        std::optional<std::unique_ptr<StatementNode>> next_statement = std::nullopt;
        const bool is_scoped = std::prev(statement_tokens.second)->token == TOK_COLON;
        if (is_scoped) {
            // --- SCOPED STATEMENT (IF, LOOPS, CATCH-BLOCK, SWITCH) ---
            next_statement = create_scoped_statement(scope, scope_segment, line_it, body, body_statements);
            scope_segment++;
        } else {
            next_statement = create_statement(scope, scope_segment, statement_tokens);
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
