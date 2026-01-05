#include "analyzer/analyzer.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/definitions/test_node.hpp"
#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/data_access_node.hpp"
#include "parser/ast/expressions/group_expression_node.hpp"
#include "parser/ast/expressions/grouped_data_access_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/expressions/optional_chain_node.hpp"
#include "parser/ast/expressions/optional_unwrap_node.hpp"
#include "parser/ast/expressions/range_expression_node.hpp"
#include "parser/ast/expressions/string_interpolation_node.hpp"
#include "parser/ast/expressions/switch_expression.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/unary_op_expression.hpp"
#include "parser/ast/expressions/variant_extraction_node.hpp"
#include "parser/ast/expressions/variant_unwrap_node.hpp"
#include "parser/ast/statements/array_assignment_node.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
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
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/stacked_assignment.hpp"
#include "parser/ast/statements/stacked_grouped_assignment.hpp"
#include "parser/ast/statements/switch_statement.hpp"
#include "parser/ast/statements/throw_node.hpp"
#include "parser/ast/statements/unary_op_statement.hpp"
#include "parser/ast/statements/while_node.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/variant_type.hpp"
#include "profiler.hpp"

#include <cassert>

Analyzer::Result Analyzer::analyze_file(const FileNode *file) {
    PROFILE_SCOPE("analyze '" + file->file_name + "'");
    Context ctx = Context{
        .level = ContextLevel::INTERNAL,
        .file_name = file->file_name,
        .line = 1,
        .column = 1,
        .length = 0,
    };
    Result result = Result::OK;
    for (const auto &node : file->file_namespace->public_symbols.definitions) {
        ctx.line = node->line;
        ctx.column = node->column;
        ctx.length = node->length;
        result = analyze_definition(ctx, node.get());
        if (result != Result::OK) {
            break;
        }
    }
    return result;
}

Analyzer::Result Analyzer::analyze_definition(const Context &ctx, const DefinitionNode *definition) {
    Result result = Result::OK;
    switch (definition->get_variation()) {
        case DefinitionNode::Variation::DATA: {
            const auto *node = definition->as<DataNode>();
            for (size_t i = 0; i < node->fields.size(); i++) {
                const auto &field = node->fields.at(i);
                Context local_ctx = ctx;
                local_ctx.line = ctx.line + i + 1;
                local_ctx.column = 4;
                result = analyze_type(local_ctx, field.second);
                if (result != Result::OK) {
                    break;
                }
            }
            break;
        }
        case DefinitionNode::Variation::ENTITY:
            break;
        case DefinitionNode::Variation::ENUM:
            break;
        case DefinitionNode::Variation::ERROR:
            break;
        case DefinitionNode::Variation::FUNC:
            break;
        case DefinitionNode::Variation::FUNCTION: {
            const auto *node = definition->as<FunctionNode>();
            Context local_ctx = ctx;
            local_ctx.level = node->is_extern ? ContextLevel::EXTERNAL : ContextLevel::INTERNAL;
            if (node->is_extern) {
                // 7 characters for 'extern '
                local_ctx.column += 7;
            }
            // 4 characters for 'def ' + name + 1 character for '('
            local_ctx.column += node->name.length() + 5;
            // Analyze all parameter types
            for (const auto &param : node->parameters) {
                if (std::get<2>(param)) {
                    local_ctx.column += 4; // Skip 'mut '
                }
                result = analyze_type(local_ctx, std::get<0>(param));
                switch (result) {
                    case Result::OK:
                        break;
                    case Result::ERR_HANDLED:
                        assert(false);
                    case Result::ERR_PTR_NOT_ALLOWED_IN_NON_EXTERN_CONTEXT:
                        THROW_ERR(                                                                //
                            ErrPtrNotAllowedInInternalFunctionDefinition, ERR_ANALYZING,          //
                            std::get<0>(param), node->file_hash, local_ctx.line, local_ctx.column //
                        );
                        return Result::ERR_HANDLED;
                }
                local_ctx.column += std::get<0>(param)->to_string().length(); // Skip type
                local_ctx.column += std::get<1>(param).length();              // Skip identifier
                local_ctx.column += 2;                                        // Skip ', '
            }
            // Analyze all return types
            // local_ctx.column += ;
            for (const auto &ret : node->return_types) {
                result = analyze_type(local_ctx, ret);
                switch (result) {
                    case Result::OK:
                        break;
                    case Result::ERR_HANDLED:
                        assert(false);
                    case Result::ERR_PTR_NOT_ALLOWED_IN_NON_EXTERN_CONTEXT:
                        THROW_ERR(                                                       //
                            ErrPtrNotAllowedInInternalFunctionDefinition, ERR_ANALYZING, //
                            ret, node->file_hash, local_ctx.line, local_ctx.column       //
                        );
                        return Result::ERR_HANDLED;
                }
            }
            if (node->scope.has_value()) {
                result = analyze_scope(local_ctx, node->scope.value().get());
                if (result != Result::OK) {
                    return result;
                }
            }
            break;
        }
        case DefinitionNode::Variation::IMPORT:
            break;
        case DefinitionNode::Variation::LINK:
            break;
        case DefinitionNode::Variation::TEST: {
            const auto *node = definition->as<TestNode>();
            result = analyze_scope(ctx, node->scope.get());
            break;
        }
        case DefinitionNode::Variation::VARIANT: {
            const auto *node = definition->as<VariantNode>();
            for (const auto &type : node->possible_types) {
                result = analyze_type(ctx, type.second);
                if (result != Result::OK) {
                    break;
                }
            }
            break;
        }
    }
    return result;
}

Analyzer::Result Analyzer::analyze_scope(const Context &ctx, const Scope *scope) {
    Result result = Result::OK;
    for (const auto &statement : scope->body) {
        result = analyze_statement(ctx, statement.get());
        if (result != Result::OK) {
            break;
        }
    }
    return result;
}

Analyzer::Result Analyzer::analyze_statement(const Context &ctx, const StatementNode *statement) {
    Result result = Result::OK;
    switch (statement->get_variation()) {
        case StatementNode::Variation::ARRAY_ASSIGNMENT: {
            const auto *node = statement->as<ArrayAssignmentNode>();
            for (const auto &index_expr : node->indexing_expressions) {
                result = analyze_expression(ctx, index_expr.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            result = analyze_expression(ctx, node->expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::ASSIGNMENT: {
            const auto *node = statement->as<AssignmentNode>();
            result = analyze_expression(ctx, node->expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::BREAK:
            break;
        case StatementNode::Variation::CALL: {
            const auto *node = statement->as<CallNodeStatement>();
            for (const auto &arg : node->arguments) {
                Context local_ctx = ctx;
                if (node->function->is_extern) {
                    local_ctx.level = ContextLevel::EXTERNAL;
                } else {
                    local_ctx.level = ContextLevel::INTERNAL;
                }
                result = analyze_expression(local_ctx, arg.first.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case StatementNode::Variation::CATCH: {
            const auto *node = statement->as<CatchNode>();
            result = analyze_scope(ctx, node->scope.get());
            if (result != Result::OK) {
                goto fail;
            }
            // Analyze the call node inside catch
            for (const auto &arg : node->call_node->arguments) {
                result = analyze_expression(ctx, arg.first.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case StatementNode::Variation::CONTINUE:
            break;
        case StatementNode::Variation::DATA_FIELD_ASSIGNMENT: {
            const auto *node = statement->as<DataFieldAssignmentNode>();
            result = analyze_expression(ctx, node->expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::DECLARATION: {
            const auto *node = statement->as<DeclarationNode>();
            if (node->initializer.has_value()) {
                result = analyze_expression(ctx, node->initializer.value().get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case StatementNode::Variation::DO_WHILE: {
            const auto *node = statement->as<DoWhileNode>();
            result = analyze_expression(ctx, node->condition.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_scope(ctx, node->scope.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::ENHANCED_FOR_LOOP: {
            const auto *node = statement->as<EnhForLoopNode>();
            result = analyze_expression(ctx, node->iterable.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_scope(ctx, node->body.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::FOR_LOOP: {
            const auto *node = statement->as<ForLoopNode>();
            result = analyze_scope(ctx, node->definition_scope.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_expression(ctx, node->condition.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_statement(ctx, node->looparound.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_scope(ctx, node->body.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::GROUP_ASSIGNMENT: {
            const auto *node = statement->as<GroupAssignmentNode>();
            result = analyze_expression(ctx, node->expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::GROUP_DECLARATION: {
            const auto *node = statement->as<GroupDeclarationNode>();
            result = analyze_expression(ctx, node->initializer.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::GROUPED_DATA_FIELD_ASSIGNMENT: {
            const auto *node = statement->as<GroupedDataFieldAssignmentNode>();
            result = analyze_expression(ctx, node->expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::IF: {
            const auto *node = statement->as<IfNode>();
            result = analyze_expression(ctx, node->condition.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_scope(ctx, node->then_scope.get());
            if (result != Result::OK) {
                goto fail;
            }
            if (node->else_scope.has_value()) {
                if (std::holds_alternative<std::shared_ptr<Scope>>(node->else_scope.value())) {
                    result = analyze_scope(ctx, std::get<std::shared_ptr<Scope>>(node->else_scope.value()).get());
                    if (result != Result::OK) {
                        goto fail;
                    }
                } else {
                    result = analyze_statement(ctx, std::get<std::unique_ptr<IfNode>>(node->else_scope.value()).get());
                    if (result != Result::OK) {
                        goto fail;
                    }
                }
            }
            break;
        }
        case StatementNode::Variation::RETURN: {
            const auto *node = statement->as<ReturnNode>();
            if (node->return_value.has_value()) {
                result = analyze_expression(ctx, node->return_value.value().get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case StatementNode::Variation::STACKED_ASSIGNMENT: {
            const auto *node = statement->as<StackedAssignmentNode>();
            result = analyze_expression(ctx, node->base_expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_expression(ctx, node->expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::STACKED_ARRAY_ASSIGNMENT: {
            const auto *node = statement->as<StackedArrayAssignmentNode>();
            result = analyze_expression(ctx, node->base_expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            for (auto &expr : node->indexing_expressions) {
                result = analyze_expression(ctx, expr.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            result = analyze_expression(ctx, node->expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::STACKED_GROUPED_ASSIGNMENT: {
            const auto *node = statement->as<StackedGroupedAssignmentNode>();
            result = analyze_expression(ctx, node->base_expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_expression(ctx, node->expression.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::SWITCH: {
            const auto *node = statement->as<SwitchStatement>();
            result = analyze_expression(ctx, node->switcher.get());
            if (result != Result::OK) {
                goto fail;
            }
            for (const auto &branch : node->branches) {
                for (const auto &match : branch.matches) {
                    result = analyze_expression(ctx, match.get());
                    if (result != Result::OK) {
                        goto fail;
                    }
                }
                result = analyze_scope(ctx, branch.body.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case StatementNode::Variation::THROW: {
            const auto *node = statement->as<ThrowNode>();
            result = analyze_expression(ctx, node->throw_value.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::UNARY_OP: {
            const auto *node = statement->as<UnaryOpStatement>();
            result = analyze_expression(ctx, node->operand.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case StatementNode::Variation::WHILE: {
            const auto *node = statement->as<WhileNode>();
            result = analyze_expression(ctx, node->condition.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_scope(ctx, node->scope.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
    }
    return result;
fail:
    // Check error and print if it's one of the errors we care about
    switch (result) {
        case Result::OK:
            [[fallthrough]];
        case Result::ERR_PTR_NOT_ALLOWED_IN_NON_EXTERN_CONTEXT:
            // Should be unreachable code
            assert(false);
        case Result::ERR_HANDLED:
            // No further printing needed
            break;
    }
    return result;
}

Analyzer::Result Analyzer::analyze_expression(const Context &ctx, const ExpressionNode *expression) {
    // Check if the type is a pointer type and if we are not in a context which allows pointer types
    Result result = analyze_type(ctx, expression->type);
    if (result != Result::OK) {
        goto fail;
    }

    switch (expression->get_variation()) {
        case ExpressionNode::Variation::ARRAY_ACCESS: {
            const auto *node = expression->as<ArrayAccessNode>();
            result = analyze_expression(ctx, node->base_expr.get());
            if (result != Result::OK) {
                goto fail;
            }
            for (const auto &index_expr : node->indexing_expressions) {
                result = analyze_expression(ctx, index_expr.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case ExpressionNode::Variation::ARRAY_INITIALIZER: {
            const auto *node = expression->as<ArrayInitializerNode>();
            for (const auto &length_expr : node->length_expressions) {
                result = analyze_expression(ctx, length_expr.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            result = analyze_expression(ctx, node->initializer_value.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::BINARY_OP: {
            const auto *node = expression->as<BinaryOpNode>();
            result = analyze_expression(ctx, node->left.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_expression(ctx, node->right.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::CALL: {
            const auto *node = expression->as<CallNodeExpression>();
            Context local_ctx = ctx;
            if (node->function->is_extern) {
                local_ctx.level = ContextLevel::EXTERNAL;
            } else {
                local_ctx.level = ContextLevel::INTERNAL;
            }
            for (const auto &arg : node->arguments) {
                result = analyze_expression(ctx, arg.first.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case ExpressionNode::Variation::DATA_ACCESS: {
            const auto *node = expression->as<DataAccessNode>();
            result = analyze_expression(ctx, node->base_expr.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::DEFAULT:
            break;
        case ExpressionNode::Variation::GROUP_EXPRESSION: {
            const auto *node = expression->as<GroupExpressionNode>();
            for (const auto &expr : node->expressions) {
                result = analyze_expression(ctx, expr.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case ExpressionNode::Variation::GROUPED_DATA_ACCESS: {
            const auto *node = expression->as<GroupedDataAccessNode>();
            result = analyze_expression(ctx, node->base_expr.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::INITIALIZER: {
            const auto *node = expression->as<InitializerNode>();
            for (const auto &arg : node->args) {
                result = analyze_expression(ctx, arg.get());
                if (result != Result::OK) {
                    goto fail;
                }
            }
            break;
        }
        case ExpressionNode::Variation::LITERAL:
            break;
        case ExpressionNode::Variation::OPTIONAL_CHAIN: {
            const auto *node = expression->as<OptionalChainNode>();
            result = analyze_expression(ctx, node->base_expr.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::OPTIONAL_UNWRAP: {
            const auto *node = expression->as<OptionalUnwrapNode>();
            result = analyze_expression(ctx, node->base_expr.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::RANGE_EXPRESSION: {
            const auto *node = expression->as<RangeExpressionNode>();
            result = analyze_expression(ctx, node->lower_bound.get());
            if (result != Result::OK) {
                goto fail;
            }
            result = analyze_expression(ctx, node->upper_bound.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::STRING_INTERPOLATION: {
            const auto *node = expression->as<StringInterpolationNode>();
            for (const auto &content : node->string_content) {
                if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(content)) {
                    const auto &expression_node = std::get<std::unique_ptr<ExpressionNode>>(content);
                    result = analyze_expression(ctx, expression_node.get());
                    if (result != Result::OK) {
                        goto fail;
                    }
                }
            }
            break;
        }
        case ExpressionNode::Variation::SWITCH_EXPRESSION: {
            const auto *node = expression->as<SwitchExpression>();
            result = analyze_expression(ctx, node->switcher.get());
            if (result != Result::OK) {
                goto fail;
            }
            for (const auto &branch : node->branches) {
                result = analyze_expression(ctx, branch.expr.get());
                if (result != Result::OK) {
                    goto fail;
                }
                for (const auto &match : branch.matches) {
                    result = analyze_expression(ctx, match.get());
                    if (result != Result::OK) {
                        goto fail;
                    }
                }
            }
            break;
        }
        case ExpressionNode::Variation::SWITCH_MATCH:
            break;
        case ExpressionNode::Variation::TYPE_CAST: {
            const auto *node = expression->as<TypeCastNode>();
            result = analyze_expression(ctx, node->expr.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::TYPE:
            break;
        case ExpressionNode::Variation::UNARY_OP: {
            const auto *node = expression->as<UnaryOpExpression>();
            result = analyze_expression(ctx, node->operand.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::VARIABLE:
            break;
        case ExpressionNode::Variation::VARIANT_EXTRACTION: {
            const auto *node = expression->as<VariantExtractionNode>();
            result = analyze_expression(ctx, node->base_expr.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
        case ExpressionNode::Variation::VARIANT_UNWRAP: {
            const auto *node = expression->as<VariantUnwrapNode>();
            result = analyze_expression(ctx, node->base_expr.get());
            if (result != Result::OK) {
                goto fail;
            }
            break;
        }
    }
    return result;
fail:
    switch (result) {
        case Result::OK:
            [[fallthrough]];
        case Result::ERR_HANDLED:
            // Those cases should not be possible at this stage
            assert(false);
            __builtin_unreachable();
        case Result::ERR_PTR_NOT_ALLOWED_IN_NON_EXTERN_CONTEXT:
            THROW_ERR(ErrPtrNotAllowedInNonExternContext, ERR_ANALYZING, expression);
            return Result::ERR_HANDLED;
    }
    __builtin_unreachable();
}

Analyzer::Result Analyzer::analyze_type(const Context &ctx, const std::shared_ptr<Type> &type_to_analyze) {
    Result result = Result::OK;
    switch (type_to_analyze->get_variation()) {
        case Type::Variation::ALIAS: {
            const auto *alias_type = type_to_analyze->as<AliasType>();
            result = analyze_type(ctx, alias_type->type);
            break;
        }
        case Type::Variation::ARRAY: {
            const auto *array_type = type_to_analyze->as<ArrayType>();
            result = analyze_type(ctx, array_type->type);
            break;
        }
        case Type::Variation::DATA:
            break;
        case Type::Variation::ENTITY:
            break;
        case Type::Variation::ENUM:
            break;
        case Type::Variation::ERROR_SET:
            break;
        case Type::Variation::FUNC:
            break;
        case Type::Variation::GROUP: {
            const auto *group_type = type_to_analyze->as<GroupType>();
            for (const auto &type : group_type->types) {
                result = analyze_type(ctx, type);
                if (result != Result::OK) {
                    break;
                }
            }
            break;
        }
        case Type::Variation::MULTI:
            break;
        case Type::Variation::OPTIONAL: {
            const auto *optional_type = type_to_analyze->as<OptionalType>();
            result = analyze_type(ctx, optional_type->base_type);
            break;
        }
        case Type::Variation::POINTER: {
            // const auto *pointer_type = type_to_analyze->as<PointerType>();
            if (ctx.level == ContextLevel::INTERNAL) {
                return Result::ERR_PTR_NOT_ALLOWED_IN_NON_EXTERN_CONTEXT;
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
                result = analyze_type(ctx, type);
                if (result != Result::OK) {
                    break;
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
                result = analyze_type(ctx, type);
                if (result != Result::OK) {
                    break;
                }
            }
            break;
        }
    }
    return result;
}
