#include "analyzer/analyzer.hpp"

#include "error/error.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/definitions/test_node.hpp"
#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/callable_call_node_expression.hpp"
#include "parser/ast/expressions/data_access_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/group_expression_node.hpp"
#include "parser/ast/expressions/grouped_data_access_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/expressions/instance_call_node_expression.hpp"
#include "parser/ast/expressions/optional_chain_node.hpp"
#include "parser/ast/expressions/optional_unwrap_node.hpp"
#include "parser/ast/expressions/range_expression_node.hpp"
#include "parser/ast/expressions/string_interpolation_node.hpp"
#include "parser/ast/expressions/switch_expression.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/unary_op_expression.hpp"
#include "parser/ast/expressions/variant_extraction_node.hpp"
#include "parser/ast/expressions/variant_unwrap_node.hpp"
#include "parser/ast/namespace.hpp"
#include "parser/ast/statements/array_assignment_node.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/ast/statements/callable_call_node_statement.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/data_field_assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "parser/ast/statements/enhanced_for_loop_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/group_assignment_node.hpp"
#include "parser/ast/statements/group_declaration_node.hpp"
#include "parser/ast/statements/grouped_data_field_assignment_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/instance_call_node_statement.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/switch_statement.hpp"
#include "parser/ast/statements/throw_node.hpp"
#include "parser/ast/statements/unary_op_statement.hpp"
#include "parser/ast/statements/while_node.hpp"
#include "parser/parser.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/fn_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/variant_type.hpp"
#include "parser/type/vector_type.hpp"
#include "profiler.hpp"
#include "resolver/resolver.hpp"

bool Analyzer::analyze_file(Parser &parser) {
    PROFILE_SCOPE("analyze '" + parser.file_name + "'");
    Context ctx = Context{
        .level = ContextLevel::INTERNAL,
        .is_function_definition_context = false,
        .file_name = parser.file_name,
        .line = 1,
        .column = 1,
        .length = 0,
        .parser = parser,
        .return_type = std::nullopt,
    };
    for (auto &node : parser.file_node_ptr->file_namespace->public_symbols.definitions) {
        ctx.line = node->line;
        ctx.column = node->column;
        ctx.length = node->length;
        if (!analyze_definition(ctx, node)) {
            return false;
        }
    }
    return true;
}

bool Analyzer::analyze_definition(const Context &ctx, std::unique_ptr<DefinitionNode> &definition) {
    switch (definition->get_variation()) {
        case DefinitionNode::Variation::DATA: {
            auto *node = definition->as<DataNode>();
            for (size_t i = 0; i < node->fields.size(); i++) {
                const auto &field = node->fields.at(i);
                Context local_ctx = ctx;
                local_ctx.line = ctx.line + i + 1;
                local_ctx.column = 4;
                if (!analyze_type(local_ctx, field.type)) {
                    return false;
                }
            }
            break;
        }
        case DefinitionNode::Variation::OBJECT:
            break;
        case DefinitionNode::Variation::ENUM:
            break;
        case DefinitionNode::Variation::ERROR:
            break;
        case DefinitionNode::Variation::FUNC:
            break;
        case DefinitionNode::Variation::FUNCTION: {
            auto *node = definition->as<FunctionNode>();
            Context local_ctx = ctx;
            local_ctx.level = node->is_extern ? ContextLevel::EXTERNAL : ContextLevel::INTERNAL;
            if (node->is_extern) {
                // 7 characters for 'extern '
                local_ctx.column += 7;
            }
            // 4 characters for 'def ' + name + 1 character for '('
            local_ctx.column += node->name.length() + 5;
            // Pointer types in the signature are reported as being part of a non-extern function definition
            local_ctx.is_function_definition_context = true;
            // Analyze all parameter types
            for (const auto &param : node->parameters) {
                if (param.is_mutable) {
                    local_ctx.column += 4; // Skip 'mut '
                }
                if (!analyze_type(local_ctx, param.type)) {
                    return false;
                }
                local_ctx.column += param.type->to_string().length(); // Skip type
                local_ctx.column += param.name.length();              // Skip identifier
                local_ctx.column += 2;                                // Skip ', '
            }
            // Analyze all return types
            // local_ctx.column += ;
            for (const auto &ret : node->return_types) {
                if (!analyze_type(local_ctx, ret)) {
                    return false;
                }
            }
            local_ctx.is_function_definition_context = false;
            if (node->scope.has_value()) {
                // The function scope holds the return type in the `flint.return_type` pseudo-variable, use it to validate return statements
                local_ctx.return_type = node->scope.value()->get_variable_type("flint.return_type");
                if (!analyze_scope(local_ctx, *node->scope.value())) {
                    return false;
                }
            }
            break;
        }
        case DefinitionNode::Variation::IMPORT:
            break;
        case DefinitionNode::Variation::INTERFACE:
            break;
        case DefinitionNode::Variation::TEST: {
            auto *node = definition->as<TestNode>();
            if (!analyze_scope(ctx, *node->scope)) {
                return false;
            }
            break;
        }
        case DefinitionNode::Variation::VARIANT: {
            auto *node = definition->as<VariantNode>();
            for (const auto &type : node->possible_types) {
                if (!analyze_type(ctx, type.second)) {
                    return false;
                }
            }
            break;
        }
    }
    return true;
}

bool Analyzer::analyze_scope(const Context &ctx, Scope &scope) {
    for (auto &statement : scope.body) {
        if (!analyze_statement(ctx, *statement)) {
            return false;
        }
    }
    return true;
}

bool Analyzer::analyze_statement(const Context &ctx, StatementNode &statement) {
    Context local_ctx = ctx;
    local_ctx.line = statement.line;
    local_ctx.column = statement.column;
    local_ctx.length = statement.length;
    switch (statement.get_variation()) {
        case StatementNode::Variation::ARRAY_ASSIGNMENT: {
            auto *node = statement.as<ArrayAssignmentNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
            for (auto &index_expr : node->indexing_expressions) {
                if (!analyze_expression(local_ctx, index_expr)) {
                    return false;
                }
                // Range expressions handle their own bound types
                if (index_expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
                    continue;
                }
                if (!Analyzer::Castability::check_castability(local_ctx.parser, u64_ty, index_expr)) {
                    THROW_ERR(                                                                             //
                        ErrExprTypeMismatch, ERR_ANALYZING, index_expr->file_hash,                         //
                        index_expr->line, index_expr->column, index_expr->length, u64_ty, index_expr->type //
                    );
                    return false;
                }
            }
            if (!analyze_expression(local_ctx, node->expression)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::ASSIGNMENT: {
            auto *node = statement.as<AssignmentNode>();
            if (!analyze_expression(local_ctx, node->expression, node->type)) {
                return false;
            }
            if (!Analyzer::Castability::check_castability(local_ctx.parser, node->type, node->expression)) {
                THROW_ERR(                                                                                   //
                    ErrExprTypeMismatch, ERR_ANALYZING, node->expression->file_hash, node->expression->line, //
                    node->expression->column, node->expression->length, node->type, node->expression->type   //
                );
                return false;
            }
            break;
        }
        case StatementNode::Variation::BREAK:
            break;
        case StatementNode::Variation::CALL: {
            auto *node = statement.as<CallNodeStatement>();
            for (auto &arg : node->arguments) {
                if (node->function->is_extern) {
                    local_ctx.level = ContextLevel::EXTERNAL;
                } else {
                    local_ctx.level = ContextLevel::INTERNAL;
                }
                if (!analyze_expression(local_ctx, arg.first)) {
                    return false;
                }
            }
            // Cast all arguments to the parameter types of the called function
            for (size_t i = 0; i < node->arguments.size(); i++) {
                auto &arg = node->arguments.at(i);
                const auto &param_type = node->function->parameters.at(i).type;
                if (!Analyzer::Castability::check_castability(local_ctx.parser, param_type, arg.first)) {
                    THROW_ERR(                                                                     //
                        ErrExprTypeMismatch, ERR_ANALYZING, arg.first->file_hash, arg.first->line, //
                        arg.first->column, arg.first->length, param_type, arg.first->type          //
                    );
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::CALLABLE_CALL: {
            auto *node = statement.as<CallableCallNodeStatement>();
            for (auto &arg : node->arguments) {
                if (!analyze_expression(local_ctx, arg.first)) {
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::CATCH: {
            auto *node = statement.as<CatchNode>();
            if (!analyze_scope(local_ctx, *node->scope)) {
                return false;
            }
            // Analyze the call node inside catch
            for (auto &arg : node->call_node->arguments) {
                if (!analyze_expression(local_ctx, arg.first)) {
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::CONTINUE:
            break;
        case StatementNode::Variation::DATA_FIELD_ASSIGNMENT: {
            auto *node = statement.as<DataFieldAssignmentNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            if (!analyze_expression(local_ctx, node->expression)) {
                return false;
            }
            if (!Analyzer::Castability::check_castability(local_ctx.parser, node->field_type, node->expression)) {
                THROW_ERR(                                                                                       //
                    ErrExprTypeMismatch, ERR_ANALYZING, node->expression->file_hash, node->expression->line,     //
                    node->expression->column, node->expression->length, node->field_type, node->expression->type //
                );
                return false;
            }
            break;
        }
        case StatementNode::Variation::DECLARATION: {
            auto *node = statement.as<DeclarationNode>();
            if (node->initializer.has_value()) {
                if (!analyze_expression(local_ctx, node->initializer.value())) {
                    return false;
                }
                if (!Analyzer::Castability::check_castability(local_ctx.parser, node->type, node->initializer.value())) {
                    THROW_ERR(                                                                                                            //
                        ErrExprTypeMismatch, ERR_ANALYZING, node->initializer.value()->file_hash, node->initializer.value()->line,        //
                        node->initializer.value()->column, node->initializer.value()->length, node->type, node->initializer.value()->type //
                    );
                    return false;
                }
            }
            if (!analyze_type(local_ctx, node->type)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::DO_WHILE: {
            auto *node = statement.as<DoWhileNode>();
            if (!analyze_expression(local_ctx, node->condition)) {
                return false;
            }
            if (!analyze_scope(local_ctx, *node->scope)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::ENHANCED_FOR_LOOP: {
            auto *node = statement.as<EnhForLoopNode>();
            if (!analyze_expression(local_ctx, node->iterable)) {
                return false;
            }
            if (!analyze_scope(local_ctx, *node->body)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::FOR_LOOP: {
            auto *node = statement.as<ForLoopNode>();
            if (!analyze_scope(local_ctx, *node->definition_scope)) {
                return false;
            }
            if (!analyze_expression(local_ctx, node->condition)) {
                return false;
            }
            if (!analyze_statement(local_ctx, *node->looparound)) {
                return false;
            }
            if (!analyze_scope(local_ctx, *node->body)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::GROUP_ASSIGNMENT: {
            auto *node = statement.as<GroupAssignmentNode>();
            if (!analyze_expression(local_ctx, node->expression)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::GROUP_DECLARATION: {
            auto *node = statement.as<GroupDeclarationNode>();
            if (!analyze_expression(local_ctx, node->initializer)) {
                return false;
            }
            std::vector<std::shared_ptr<Type>> group_types;
            for (const auto &variable : node->variables) {
                group_types.emplace_back(variable.first);
            }
            std::shared_ptr<Type> group_type = std::make_shared<GroupType>(group_types);
            if (!Type::add_type(group_type)) {
                group_type = Type::get_type_from_str(group_type->to_string()).value();
            }
            if (!Analyzer::Castability::check_castability(local_ctx.parser, group_type, node->initializer)) {
                THROW_ERR(                                                                                     //
                    ErrExprTypeMismatch, ERR_ANALYZING, node->initializer->file_hash, node->initializer->line, //
                    node->initializer->column, node->initializer->length, group_type, node->initializer->type  //
                );
                return false;
            }
            break;
        }
        case StatementNode::Variation::GROUPED_ARRAY_ASSIGNMENT: {
            auto *node = statement.as<GroupedArrayAssignmentNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
            std::shared_ptr<Type> indexing_ty = u64_ty;
            if (node->base_expr->type->get_variation() == Type::Variation::ARRAY) {
                const auto *array_ty = node->base_expr->type->as<ArrayType>();
                if (array_ty->dimensionality > 1) {
                    std::vector<std::shared_ptr<Type>> indexing_group_types;
                    for (size_t i = 0; i < array_ty->dimensionality; i++) {
                        indexing_group_types.emplace_back(u64_ty);
                    }
                    indexing_ty = std::make_shared<GroupType>(indexing_group_types);
                    if (!Type::add_type(indexing_ty)) {
                        indexing_ty = Type::get_type_from_str(indexing_ty->to_string()).value();
                    }
                }
            }
            for (auto &index_expr : node->indexing_expressions) {
                if (!analyze_expression(local_ctx, index_expr)) {
                    return false;
                }
                // Range expressions handle their own bound types
                if (index_expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
                    continue;
                }
                if (!Analyzer::Castability::check_castability(local_ctx.parser, indexing_ty, index_expr)) {
                    THROW_ERR(                                                                       //
                        ErrExprTypeMismatch, ERR_ANALYZING, index_expr->file_hash, index_expr->line, //
                        index_expr->column, index_expr->length, indexing_ty, index_expr->type        //
                    );
                    return false;
                }
            }
            if (!analyze_expression(local_ctx, node->expression)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::GROUPED_DATA_FIELD_ASSIGNMENT: {
            auto *node = statement.as<GroupedDataFieldAssignmentNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            if (!analyze_expression(local_ctx, node->expression)) {
                return false;
            }
            std::shared_ptr<Type> group_type = std::make_shared<GroupType>(node->field_types);
            if (!Type::add_type(group_type)) {
                group_type = Type::get_type_from_str(group_type->to_string()).value();
            }
            if (!Analyzer::Castability::check_castability(local_ctx.parser, group_type, node->expression)) {
                THROW_ERR(                                                                                   //
                    ErrExprTypeMismatch, ERR_ANALYZING, node->expression->file_hash, node->expression->line, //
                    node->expression->column, node->expression->length, group_type, node->expression->type   //
                );
                return false;
            }
            break;
        }
        case StatementNode::Variation::IF: {
            auto *node = statement.as<IfNode>();
            if (!analyze_expression(local_ctx, node->condition)) {
                return false;
            }
            if (!analyze_scope(local_ctx, *node->then_scope)) {
                return false;
            }
            if (node->else_scope.has_value()) {
                if (std::holds_alternative<std::shared_ptr<Scope>>(node->else_scope.value())) {
                    if (!analyze_scope(local_ctx, *std::get<std::shared_ptr<Scope>>(node->else_scope.value()))) {
                        return false;
                    }
                } else {
                    if (!analyze_statement(local_ctx, *std::get<std::unique_ptr<IfNode>>(node->else_scope.value()))) {
                        return false;
                    }
                }
            }
            break;
        }
        case StatementNode::Variation::INSTANCE_CALL: {
            auto *node = statement.as<InstanceCallNodeStatement>();
            for (auto &arg : node->arguments) {
                if (node->function->is_extern) {
                    local_ctx.level = ContextLevel::EXTERNAL;
                } else {
                    local_ctx.level = ContextLevel::INTERNAL;
                }
                if (!analyze_expression(local_ctx, arg.first)) {
                    return false;
                }
            }
            // Cast all arguments to the parameter types of the called function
            for (size_t i = 0; i < node->arguments.size(); i++) {
                const auto &param_type = node->function->parameters.at(i).type;
                auto &arg = node->arguments.at(i);
                if (!Analyzer::Castability::check_castability(local_ctx.parser, param_type, arg.first)) {
                    THROW_ERR(                                                                     //
                        ErrExprTypeMismatch, ERR_ANALYZING, arg.first->file_hash, arg.first->line, //
                        arg.first->column, arg.first->length, param_type, arg.first->type          //
                    );
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::RETURN: {
            auto *node = statement.as<ReturnNode>();
            if (!node->return_value.has_value()) {
                break;
            }
            if (!analyze_expression(local_ctx, node->return_value.value(), ctx.return_type)) {
                return false;
            }
            if (local_ctx.return_type.has_value()                                                                                   //
                && !Analyzer::Castability::check_castability(local_ctx.parser, ctx.return_type.value(), node->return_value.value()) //
            ) {
                const auto &ret_val = node->return_value.value();
                THROW_ERR(                                                                   //
                    ErrExprTypeMismatch, ERR_ANALYZING, ret_val->file_hash, ret_val->line,   //
                    ret_val->column, ret_val->length, ctx.return_type.value(), ret_val->type //
                );
                return false;
            }
            break;
        }
        case StatementNode::Variation::SWITCH: {
            auto *node = statement.as<SwitchStatement>();
            if (!analyze_expression(local_ctx, node->switcher)) {
                return false;
            }
            // Only primitive switchers have their matches validated and cast, the other switch kinds (enum, optional, variant) use
            // specialized match nodes which must not be touched
            const bool is_primitive_switcher = node->switcher->type->get_variation() == Type::Variation::PRIMITIVE;
            for (auto &branch : node->branches) {
                for (auto &match : branch.matches) {
                    if (!analyze_expression(local_ctx, match)) {
                        return false;
                    }
                    if (is_primitive_switcher && !Analyzer::Castability::check_castability(local_ctx.parser, node->switcher->type, match)) {
                        THROW_ERR(                                                             //
                            ErrExprTypeMismatch, ERR_ANALYZING, match->file_hash, match->line, //
                            match->column, match->length, node->switcher->type, match->type    //
                        );
                        return false;
                    }
                }
                if (!analyze_scope(local_ctx, *branch.body)) {
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::THROW: {
            auto *node = statement.as<ThrowNode>();
            if (!analyze_expression(local_ctx, node->throw_value)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::UNARY_OP: {
            auto *node = statement.as<UnaryOpStatement>();
            if (!analyze_expression(local_ctx, node->operand)) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::WHILE: {
            auto *node = statement.as<WhileNode>();
            if (!analyze_expression(local_ctx, node->condition)) {
                return false;
            }
            if (!analyze_scope(local_ctx, *node->scope)) {
                return false;
            }
            break;
        }
    }
    return true;
}

bool Analyzer::analyze_binop(const Analyzer::Context &ctx, std::unique_ptr<ExpressionNode> &expr) {
    auto *const node = expr->as<BinaryOpNode>();
    if (!Analyzer::analyze_expression(ctx, node->left)) {
        return false;
    }
    if (!Analyzer::analyze_expression(ctx, node->right)) {
        return false;
    }

    // Match the two operands of the binary operation against each other, coercing them in place
    const std::string lhs_type_str = node->left->type->to_string();
    const std::string rhs_type_str = node->right->type->to_string();
    switch (Castability::match_binop_operands(ctx.parser, node->operator_token, node->left, node->right)) {
        case Castability::BinopMatchResult::OK:
            break;
        case Castability::BinopMatchResult::TYPE_MISMATCH:
            if (node->operator_token == TOK_OPT_DEFAULT) {
                // ?? operator not possible on non-optional type
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            THROW_ERR(                                                                                           //
                ErrExprBinopTypeMismatch, ERR_PARSING, ctx.parser.file_node_ptr->file_namespace->namespace_hash, //
                node->line, node->column, node->length, node->operator_token, lhs_type_str, rhs_type_str         //
            );
            return false;
        case Castability::BinopMatchResult::OPT_DEFAULT_MISMATCH: {
            const auto *lhs_opt = node->left->type->as<OptionalType>();
            THROW_ERR(                                                                                      //
                ErrExprTypeMismatch, ERR_PARSING, ctx.parser.file_node_ptr->file_namespace->namespace_hash, //
                node->line, node->column, node->length, lhs_opt->base_type, node->right->type               //
            );
            return false;
        }
    }

    // Check for const folding, and return the folded value if const folding was able to be applied
    std::optional<std::unique_ptr<ExpressionNode>> folded_result = ctx.parser.check_const_folding( //
        node->left, node->operator_token, node->right                                              //
    );
    if (folded_result.has_value()) {
        expr = std::move(folded_result.value());
        return true;
    }

    // Check it the binary operator is a `catch` keyword, if so the lhs should be a function call. We then set it's "has_catch"
    // field to true
    if (node->operator_token == TOK_CATCH) {
        switch (node->left->get_variation()) {
            default:
                // Not allowed lhs expression to catch binop
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            case ExpressionNode::Variation::CALL:
                node->left->as<CallNodeExpression>()->has_catch = true;
                break;
            case ExpressionNode::Variation::CALLABLE_CALL:
                node->left->as<CallableCallNodeExpression>()->has_catch = true;
                break;
            case ExpressionNode::Variation::INSTANCE_CALL:
                node->left->as<InstanceCallNodeExpression>()->has_catch = true;
                break;
        }
    }

    // Set the type of the binop expression itself to the base type of the optional for opt default expressions
    if (node->operator_token == TOK_OPT_DEFAULT) {
        node->type = node->left->type->as<OptionalType>()->base_type;
        return true;
    }

    // Finally check if one of the two sides are string literals, if they are they need to become a string variable
    if (node->left->type->to_string() == "type.flint.str.lit") {
        node->left = std::make_unique<TypeCastNode>(                                                       //
            node->file_hash, ASTNode::PosTriple{node->left->line, node->left->column, node->left->length}, //
            Type::get_primitive_type("str"), node->left                                                    //
        );
    }
    if (node->right->type->to_string() == "type.flint.str.lit") {
        node->right = std::make_unique<TypeCastNode>(                                                         //
            node->file_hash, ASTNode::PosTriple{node->right->line, node->right->column, node->right->length}, //
            Type::get_primitive_type("str"), node->right                                                      //
        );
    }

    // Set the type of the binary operator node
    if (Matcher::token_match(node->operator_token, Matcher::relational_binop)) {
        node->type = Type::get_primitive_type("bool");
    } else {
        node->type = node->left->type;
    }
    return true;
}

bool Analyzer::analyze_expression(                            //
    const Context &ctx,                                       //
    std::unique_ptr<ExpressionNode> &expr,                    //
    const std::optional<std::shared_ptr<Type>> &expected_type //
) {
    // Check if the type is a pointer type and if we are not in a context which allows pointer types
    Context local_ctx = ctx;
    local_ctx.line = expr->line;
    local_ctx.column = expr->column;
    local_ctx.length = expr->length;
    if (!analyze_type(local_ctx, expr->type, true)) {
        return false;
    }

    switch (expr->get_variation()) {
        case ExpressionNode::Variation::ARRAY_ACCESS: {
            auto *node = expr->as<ArrayAccessNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
            for (auto &index_expr : node->indexing_expressions) {
                if (!analyze_expression(local_ctx, index_expr)) {
                    return false;
                }
                // Range expressions handle their own bound types
                if (index_expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
                    continue;
                }
                if (!Analyzer::Castability::check_castability(local_ctx.parser, u64_ty, index_expr)) {
                    THROW_ERR(                                                                       //
                        ErrExprTypeMismatch, ERR_ANALYZING, index_expr->file_hash, index_expr->line, //
                        index_expr->column, index_expr->length, u64_ty, index_expr->type             //
                    );
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::ARRAY_INITIALIZER: {
            auto *node = expr->as<ArrayInitializerNode>();
            const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
            for (auto &length_expr : node->length_expressions) {
                if (!analyze_expression(local_ctx, length_expr)) {
                    return false;
                }
                // Range expressions handle their own bound types
                if (length_expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
                    continue;
                }
                if (!Analyzer::Castability::check_castability(local_ctx.parser, u64_ty, length_expr)) {
                    THROW_ERR(                                                                         //
                        ErrExprTypeMismatch, ERR_ANALYZING, length_expr->file_hash, length_expr->line, //
                        length_expr->column, length_expr->length, u64_ty, length_expr->type            //
                    );
                    return false;
                }
            }
            if (!analyze_expression(local_ctx, node->initializer_value)) {
                return false;
            }
            const auto *arr_type = node->type->as<ArrayType>();
            if (!Analyzer::Castability::check_castability(local_ctx.parser, arr_type->type, node->initializer_value)) {
                THROW_ERR(                                                                                                          //
                    ErrExprTypeMismatch, ERR_ANALYZING, node->initializer_value->file_hash, node->initializer_value->line,          //
                    node->initializer_value->column, node->initializer_value->length, arr_type->type, node->initializer_value->type //
                );
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::BINARY_OP: {
            if (!analyze_binop(local_ctx, expr)) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::CALL: {
            auto *node = expr->as<CallNodeExpression>();
            if (node->function->is_extern) {
                local_ctx.level = ContextLevel::EXTERNAL;
            } else {
                local_ctx.level = ContextLevel::INTERNAL;
            }
            for (auto &arg : node->arguments) {
                if (!analyze_expression(local_ctx, arg.first)) {
                    return false;
                }
            }
            // Cast all arguments to the parameter types of the called function
            for (size_t i = 0; i < node->arguments.size(); i++) {
                const auto &param_type = node->function->parameters.at(i).type;
                auto &arg = node->arguments.at(i);
                if (!Analyzer::Castability::check_castability(local_ctx.parser, param_type, arg.first)) {
                    THROW_ERR(                                                                     //
                        ErrExprTypeMismatch, ERR_ANALYZING, arg.first->file_hash, arg.first->line, //
                        arg.first->column, arg.first->length, param_type, arg.first->type          //
                    );
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::CALLABLE_CALL: {
            auto *node = expr->as<CallableCallNodeExpression>();
            local_ctx.level = ContextLevel::INTERNAL;
            for (auto &arg : node->arguments) {
                if (!analyze_expression(local_ctx, arg.first)) {
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::DATA_ACCESS: {
            auto *node = expr->as<DataAccessNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::DEFAULT:
            break;
        case ExpressionNode::Variation::GROUP_EXPRESSION: {
            auto *node = expr->as<GroupExpressionNode>();
            for (auto &expression : node->expressions) {
                if (!analyze_expression(local_ctx, expression)) {
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::FUNCTION_REFERENCE:
            break;
        case ExpressionNode::Variation::GROUPED_ARRAY_ACCESS: {
            auto *node = expr->as<GroupedArrayAccessNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            // The indexing expressions must all have the type of a group with N values where N is the dimensionality of the accessed array
            const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
            std::shared_ptr<Type> indexing_ty = u64_ty;
            if (node->base_expr->type->get_variation() == Type::Variation::ARRAY) {
                const auto *array_ty = node->base_expr->type->as<ArrayType>();
                if (array_ty->dimensionality > 1) {
                    std::vector<std::shared_ptr<Type>> indexing_group_types;
                    for (size_t i = 0; i < array_ty->dimensionality; i++) {
                        indexing_group_types.emplace_back(u64_ty);
                    }
                    indexing_ty = std::make_shared<GroupType>(indexing_group_types);
                    if (!Type::add_type(indexing_ty)) {
                        indexing_ty = Type::get_type_from_str(indexing_ty->to_string()).value();
                    }
                }
            }
            for (auto &index_expr : node->indexing_expressions) {
                if (!analyze_expression(local_ctx, index_expr)) {
                    return false;
                }
                // Range expressions handle their own bound types
                if (index_expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
                    continue;
                }
                if (!Analyzer::Castability::check_castability(local_ctx.parser, indexing_ty, index_expr)) {
                    THROW_ERR(                                                                       //
                        ErrExprTypeMismatch, ERR_ANALYZING, index_expr->file_hash, index_expr->line, //
                        index_expr->column, index_expr->length, indexing_ty, index_expr->type        //
                    );
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::GROUPED_DATA_ACCESS: {
            auto *node = expr->as<GroupedDataAccessNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::INITIALIZER: {
            auto *node = expr->as<InitializerNode>();
            for (auto &arg : node->args) {
                if (!analyze_expression(local_ctx, arg)) {
                    return false;
                }
            }
            switch (node->type->get_variation()) {
                default:
                    break;
                case Type::Variation::DATA: {
                    // Every argument needs to be castable to the type of the corresponding field
                    const auto &fields = node->type->as<DataType>()->data_node->fields;
                    for (size_t i = 0; i < node->args.size(); i++) {
                        if (node->args[i]->get_variation() == ExpressionNode::Variation::DEFAULT) {
                            continue;
                        }
                        const auto &field_type = fields.at(i).type;
                        if (!Analyzer::Castability::check_castability(local_ctx.parser, field_type, node->args[i], false)) {
                            THROW_ERR(                                                                             //
                                ErrExprTypeMismatch, ERR_ANALYZING, node->args[i]->file_hash, node->args[i]->line, //
                                node->args[i]->column, node->args[i]->length, field_type, node->args[i]->type      //
                            );
                            return false;
                        }
                    }
                    break;
                }
                case Type::Variation::OBJECT: {
                    // Every argument needs to equal the type of the corresponding data module
                    const auto *object_type = node->type->as<ObjectType>();
                    const auto &constructor_order = object_type->object_node->constructor_order;
                    for (size_t i = 0; i < node->args.size(); i++) {
                        if (node->args[i]->get_variation() == ExpressionNode::Variation::DEFAULT) {
                            continue;
                        }
                        const DataNode *data_node = object_type->object_node->data_modules.at(constructor_order.at(i)).first;
                        const Namespace *data_namespace = Resolver::get_namespace_from_hash(data_node->file_hash);
                        const std::shared_ptr<Type> data_type = data_namespace->get_type_from_str(data_node->name).value();
                        if (!node->args[i]->type->equals(data_type)) {
                            THROW_ERR(ErrExprTypeMismatch, ERR_ANALYZING, node->args[i]->file_hash, node->args[i]->line,
                                node->args[i]->column, node->args[i]->length, data_type, node->args[i]->type);
                            return false;
                        }
                    }
                    break;
                }
                case Type::Variation::VECTOR: {
                    if (node->args.size() == 1) {
                        break;
                    }
                    const std::shared_ptr<Type> base_type = node->type->as<VectorType>()->base_type;
                    for (auto &arg : node->args) {
                        if (!Analyzer::Castability::check_castability(local_ctx.parser, base_type, arg, false)) {
                            THROW_ERR(                                                        //
                                ErrExprCastInvalid, ERR_ANALYZING, arg->file_hash, arg->line, //
                                arg->column, arg->length, node->type, arg->type               //
                            );
                            return false;
                        }
                    }
                }
            }
            break;
        }
        case ExpressionNode::Variation::INLINE_ARRAY_INITIALIZER: {
            auto *node = expr->as<InlineArrayInitializerNode>();
            const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
            for (auto &length_expr : node->length_expressions) {
                if (!analyze_expression(local_ctx, length_expr)) {
                    return false;
                }
                // Range expressions handle their own bound types
                if (length_expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
                    continue;
                }
                if (!Analyzer::Castability::check_castability(local_ctx.parser, u64_ty, length_expr)) {
                    THROW_ERR(                                                                         //
                        ErrExprTypeMismatch, ERR_ANALYZING, length_expr->file_hash, length_expr->line, //
                        length_expr->column, length_expr->length, u64_ty, length_expr->type            //
                    );
                    return false;
                }
            }
            for (auto &init_expr : node->initializer_values) {
                if (!analyze_expression(local_ctx, init_expr)) {
                    return false;
                }
                if (!Analyzer::Castability::check_castability(local_ctx.parser, node->element_type, init_expr)) {
                    THROW_ERR(                                                                     //
                        ErrExprTypeMismatch, ERR_ANALYZING, init_expr->file_hash, init_expr->line, //
                        init_expr->column, init_expr->length, node->element_type, init_expr->type  //
                    );
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::INSTANCE_CALL: {
            auto *node = expr->as<InstanceCallNodeExpression>();
            if (node->function->is_extern) {
                local_ctx.level = ContextLevel::EXTERNAL;
            } else {
                local_ctx.level = ContextLevel::INTERNAL;
            }
            for (auto &arg : node->arguments) {
                if (!analyze_expression(local_ctx, arg.first)) {
                    return false;
                }
            }
            // Cast all arguments to the parameter types of the called function
            for (size_t i = 0; i < node->arguments.size(); i++) {
                auto &arg = node->arguments.at(i);
                const auto &param_type = node->function->parameters.at(i).type;
                if (!Analyzer::Castability::check_castability(local_ctx.parser, param_type, arg.first)) {
                    THROW_ERR(                                                                     //
                        ErrExprTypeMismatch, ERR_ANALYZING, arg.first->file_hash, arg.first->line, //
                        arg.first->column, arg.first->length, param_type, arg.first->type          //
                    );
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::LITERAL: {
            auto *node = expr->as<LiteralNode>();
            // Literals can contain sub-expressions (error value messages and variant constructor values) which need to be analyzed and
            // cast so that e.g. string interpolation content inside of them gets properly retyped
            if (std::holds_alternative<LitError>(node->value)) {
                LitError &lit_error = std::get<LitError>(node->value);
                if (lit_error.message.has_value()) {
                    const std::shared_ptr<Type> str_type = Type::get_primitive_type("str");
                    if (!analyze_expression(local_ctx, lit_error.message.value())) {
                        return false;
                    }
                    if (!Analyzer::Castability::check_castability(local_ctx.parser, str_type, lit_error.message.value())) {
                        THROW_ERR(                                                                //
                            ErrExprTypeMismatch, ERR_ANALYZING, expr->file_hash, expr->line,      //
                            expr->column, expr->length, str_type, lit_error.message.value()->type //
                        );
                        return false;
                    }
                }
            } else if (std::holds_alternative<LitVariant>(node->value)) {
                LitVariant &lit_variant = std::get<LitVariant>(node->value);
                if (lit_variant.expr.has_value()) {
                    if (!analyze_expression(local_ctx, lit_variant.expr.value())) {
                        return false;
                    }
                }
            }
            break;
        }
        case ExpressionNode::Variation::OPTIONAL_CHAIN: {
            auto *node = expr->as<OptionalChainNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::OPTIONAL_UNWRAP: {
            auto *node = expr->as<OptionalUnwrapNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::RANGE_EXPRESSION: {
            auto *node = expr->as<RangeExpressionNode>();
            if (!analyze_expression(local_ctx, node->lower_bound)) {
                return false;
            }
            if (!analyze_expression(local_ctx, node->upper_bound)) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::STRING_INTERPOLATION: {
            auto *node = expr->as<StringInterpolationNode>();
            const std::shared_ptr<Type> str_type = Type::get_primitive_type("str");
            for (auto &content : node->string_content) {
                if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(content)) {
                    auto &expression_node = std::get<std::unique_ptr<ExpressionNode>>(content);
                    if (!analyze_expression(local_ctx, expression_node)) {
                        return false;
                    }
                    // Cast the expression content to a str type (if it isn't already)
                    if (!Analyzer::Castability::check_castability(local_ctx.parser, str_type, expression_node)) {
                        THROW_ERR(                                                                                 //
                            ErrExprTypeMismatch, ERR_ANALYZING, expression_node->file_hash, expression_node->line, //
                            expression_node->column, expression_node->length, str_type, expression_node->type      //
                        );
                        return false;
                    }
                }
            }
            break;
        }
        case ExpressionNode::Variation::SWITCH_EXPRESSION: {
            auto *node = expr->as<SwitchExpression>();
            if (!analyze_expression(local_ctx, node->switcher)) {
                return false;
            }
            // Only primitive switchers have their matches validated and cast, the other switch kinds (enum, optional, variant) use
            // specialized match nodes which must not be touched
            const bool is_primitive_switcher = node->switcher->type->get_variation() == Type::Variation::PRIMITIVE;
            for (auto &branch : node->branches) {
                if (!analyze_expression(local_ctx, branch.expr)) {
                    return false;
                }
                for (auto &match : branch.matches) {
                    if (!analyze_expression(local_ctx, match)) {
                        return false;
                    }
                    if (is_primitive_switcher && !Analyzer::Castability::check_castability(local_ctx.parser, node->switcher->type, match)) {
                        THROW_ERR(                                                             //
                            ErrExprTypeMismatch, ERR_ANALYZING, match->file_hash, match->line, //
                            match->column, match->length, node->switcher->type, match->type    //
                        );
                        return false;
                    }
                }
            }
            // Cast all branch expressions to the common type of the switch expression. If the common type is a comptime type (e.g. `int`),
            // it needs to be resolved to a concrete type first so that the branch literals can be retyped as well. If an expected type was
            // passed down (e.g. from a return statement or an assignment) and the common type can be implicitly cast to it, the comptime
            // type adopts that expected type, otherwise it falls back to its default concrete type (i32 / f32)
            const std::string &common_type_str = node->type->to_string();
            if (common_type_str == "int" || common_type_str == "float") {
                const std::string default_type_str = common_type_str == "int" ? "i32" : "f32";
                std::string resolved_type_str = default_type_str;
                if (expected_type.has_value()) {
                    const std::string &expected_type_str = expected_type.value()->to_string();
                    if (common_type_str == "int") {
                        // The `int` comptime type can adopt every concrete primitive integer and float type
                        const bool is_valid_int_target =  //
                            expected_type_str == "u8"     //
                            || expected_type_str == "i8"  //
                            || expected_type_str == "u16" //
                            || expected_type_str == "i16" //
                            || expected_type_str == "u32" //
                            || expected_type_str == "i32" //
                            || expected_type_str == "u64" //
                            || expected_type_str == "i64" //
                            || expected_type_str == "f32" //
                            || expected_type_str == "f64";
                        if (is_valid_int_target) {
                            resolved_type_str = expected_type_str;
                        }
                    } else if (expected_type_str == "f32" || expected_type_str == "f64") {
                        // The `float` comptime type can only adopt the concrete float types
                        resolved_type_str = expected_type_str;
                    }
                }
                node->type = Type::get_primitive_type(resolved_type_str);
            }
            for (auto &branch : node->branches) {
                if (!Analyzer::Castability::check_castability(local_ctx.parser, node->type, branch.expr)) {
                    THROW_ERR(                                                                         //
                        ErrExprTypeMismatch, ERR_ANALYZING, branch.expr->file_hash, branch.expr->line, //
                        branch.expr->column, branch.expr->length, node->type, branch.expr->type        //
                    );
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::SWITCH_MATCH:
            break;
        case ExpressionNode::Variation::TYPE_CAST: {
            auto *node = expr->as<TypeCastNode>();
            if (!analyze_expression(local_ctx, node->expr)) {
                return false;
            }
            if (node->expr->type->equals(node->type)) {
                // The cast is redundant, the inner expression already has the target type
                expr = std::move(node->expr);
                break;
            }
            // Enums are allowed to be cast to strings and to integers without any further validation
            if (node->expr->type->get_variation() == Type::Variation::ENUM) {
                const std::string &to_type_str = node->type->to_string();
                const bool is_enum_int_cast = to_type_str == "u8" || to_type_str == "u16" || to_type_str == "u32" //
                    || to_type_str == "u64" || to_type_str == "i8" || to_type_str == "i16" || to_type_str == "i32" || to_type_str == "i64";
                if (to_type_str == "str" || is_enum_int_cast) {
                    break;
                }
                THROW_ERR(                                                          //
                    ErrExprCastInvalid, ERR_ANALYZING, node->file_hash, node->line, //
                    node->column, node->length, node->type, node->expr->type        //
                );
                return false;
            }
            // If the inner expression is a literal, retype the literal and remove the cast node
            if (Analyzer::Castability::resolve_comptime_type_of_expr(local_ctx.parser, node->expr, node->type)) {
                expr = std::move(node->expr);
                break;
            }
            if (!Analyzer::Castability::check_castability(local_ctx.parser, node->type, node->expr, false)) {
                THROW_ERR(                                                          //
                    ErrExprCastInvalid, ERR_ANALYZING, node->file_hash, node->line, //
                    node->column, node->length, node->type, node->expr->type        //
                );
                return false;
            }
            // Collapse the cast node if the inner expression already has the target type after the castability check
            if (node->expr->type->equals(node->type)) {
                expr = std::move(node->expr);
            }
            break;
        }
        case ExpressionNode::Variation::TYPE:
            break;
        case ExpressionNode::Variation::UNARY_OP: {
            auto *node = expr->as<UnaryOpExpression>();
            if (!analyze_expression(local_ctx, node->operand)) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::VARIABLE:
            break;
        case ExpressionNode::Variation::VARIANT_EXTRACTION: {
            auto *node = expr->as<VariantExtractionNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::VARIANT_UNWRAP: {
            auto *node = expr->as<VariantUnwrapNode>();
            if (!analyze_expression(local_ctx, node->base_expr)) {
                return false;
            }
            break;
        }
    }

    // Check if the types are implicitely type castable, if they are, wrap the expression in a TypeCastNode
    if (expected_type.has_value() && !expected_type.value()->equals(expr->type)) {
        const ASTNode::PosTriple expr_pos = ASTNode::PosTriple{
            .line = expr->line,
            .column = expr->column,
            .length = expr->length,
        };
        switch (expected_type.value()->get_variation()) {
            default: {
                if (!Castability::check_castability(local_ctx.parser, expected_type.value(), expr, true)) {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, ctx.parser.file_hash, expr_pos, expected_type.value(), expr->type);
                    return false;
                }
                break;
            }
            case Type::Variation::ERROR_SET: {
                const auto *target_error_type = expected_type.value()->as<ErrorSetType>();
                if (expr->type->get_variation() != Type::Variation::ERROR_SET) {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, ctx.parser.file_hash, expr_pos, expected_type.value(), expr->type);
                    return false;
                }
                const auto *expr_error_type = expr->type->as<ErrorSetType>();
                // The expr error set type needs to be a superset of the target error type to be castable to it, this means that the
                // expression type "extends" the target type
                std::optional<const ErrorNode *> parent_node = target_error_type->error_node;
                bool is_castable = false;
                while (parent_node.has_value()) {
                    if (parent_node.value() == expr_error_type->error_node) {
                        is_castable = true;
                        break;
                    }
                    parent_node = parent_node.value()->get_parent_node();
                }
                if (!is_castable) {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, ctx.parser.file_hash, expr_pos, expected_type.value(), expr->type);
                    return false;
                }
                expr = std::make_unique<TypeCastNode>(                          //
                    ctx.parser.file_hash, expr_pos, expected_type.value(), expr //
                );
                break;
            }
        }
    }

    return true;
}

bool Analyzer::analyze_type(                      //
    const Context &ctx,                           //
    const std::shared_ptr<Type> &type_to_analyze, //
    const bool is_empty_fixed_array_allowed       //
) {
    switch (type_to_analyze->get_variation()) {
        case Type::Variation::ALIAS: {
            const auto *alias_type = type_to_analyze->as<AliasType>();
            if (!analyze_type(ctx, alias_type->type)) {
                return false;
            }
            break;
        }
        case Type::Variation::ARRAY: {
            const auto *array_type = type_to_analyze->as<ArrayType>();
            if (!is_empty_fixed_array_allowed && array_type->is_fixed_and_empty()) {
                THROW_ERR(                                                                                                              //
                    ErrEmptyStoredFixedArray, ERR_ANALYZING, ctx.parser.file_hash, ASTNode::PosTriple{ctx.line, ctx.column, ctx.length} //
                );
                return false;
            }
            if (!analyze_type(ctx, array_type->type)) {
                return false;
            }
            break;
        }
        case Type::Variation::DATA:
            break;
        case Type::Variation::ENUM:
            break;
        case Type::Variation::ERROR_SET:
            break;
        case Type::Variation::FUNC:
            break;
        case Type::Variation::FN: {
            const auto *fn_type = type_to_analyze->as<FnType>();
            for (const auto &[type, is_mutable] : fn_type->params) {
                if (!analyze_type(ctx, type)) {
                    return false;
                }
            }
            for (const auto &type : fn_type->return_types) {
                if (!analyze_type(ctx, type)) {
                    return false;
                }
            }
            break;
        }
        case Type::Variation::GROUP: {
            const auto *group_type = type_to_analyze->as<GroupType>();
            for (const auto &type : group_type->types) {
                if (!analyze_type(ctx, type)) {
                    return false;
                }
            }
            break;
        }
        case Type::Variation::INTERFACE:
            break;
        case Type::Variation::OBJECT:
            break;
        case Type::Variation::OPAQUE:
            break;
        case Type::Variation::OPTIONAL: {
            const auto *optional_type = type_to_analyze->as<OptionalType>();
            if (!analyze_type(ctx, optional_type->base_type)) {
                return false;
            }
            break;
        }
        case Type::Variation::POINTER: {
            // void* is allowed even in non-extern places because it's the type of the 'null' literal and cannot come up anywhere else in
            // Flint, ever
            if (type_to_analyze->to_string() != "void*"                                           //
                && (ctx.level == ContextLevel::INTERNAL || ctx.level == ContextLevel::CONST_DATA) //
            ) {
                if (ctx.is_function_definition_context) {
                    THROW_ERR(                                                                            //
                        ErrPtrNotAllowedInInternalFunctionDefinition, ERR_ANALYZING,                      //
                        ctx.parser.file_hash, ctx.line, ctx.column, type_to_analyze->to_string().length() //
                    );
                    return false;
                } else {
                    THROW_ERR(ErrPtrNotAllowedInNonExternContext, ERR_ANALYZING, ctx.parser.file_hash, ctx.line, ctx.column, ctx.length);
                    return false;
                }
            }
            break;
        }
        case Type::Variation::PRIMITIVE:
            break;
        case Type::Variation::RANGE:
            break;
        case Type::Variation::TUPLE: {
            const auto *tuple_type = type_to_analyze->as<TupleType>();
            for (const auto &type : tuple_type->types) {
                if (!analyze_type(ctx, type)) {
                    return false;
                }
            }
            break;
        }
        case Type::Variation::UNKNOWN:
            break;
        case Type::Variation::VARIANT: {
            const auto *variant_type = type_to_analyze->as<VariantType>();
            if (variant_type->is_err_variant) {
                break;
            }
            if (std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                break;
            }
            const auto &types = std::get<std::vector<std::shared_ptr<Type>>>(variant_type->var_or_list);
            for (const auto &type : types) {
                if (!analyze_type(ctx, type)) {
                    return false;
                }
            }
            break;
        }
        case Type::Variation::VECTOR:
            break;
    }
    return true;
}
