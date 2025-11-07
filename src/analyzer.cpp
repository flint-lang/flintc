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
#include "parser/type/pointer_type.hpp"
#include "profiler.hpp"

#include <cassert>

bool Analyzer::analyze(const FileNode *file) {
    PROFILE_SCOPE("analyze '" + file->file_name + "'");
    for (const auto &node : file->definitions) {
        Context ctx = Context{.is_extern = false};
        if (!analyze_definition(ctx, node.get())) {
            return false;
        }
    }
    return true;
}

bool Analyzer::analyze_definition(const Context &ctx, const DefinitionNode *definition) {
    switch (definition->get_variation()) {
        case DefinitionNode::Variation::UNKNOWN_VARIATION:
            assert(false && "Unknown definition node type");
            return false;
        case DefinitionNode::Variation::DATA:
            break;
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
            if (node->scope.has_value()) {
                Context local_ctx = ctx;
                local_ctx.is_extern = node->is_extern;
                if (!analyze_scope(local_ctx, node->scope.value().get())) {
                    return false;
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
            if (!analyze_scope(ctx, node->scope.get())) {
                return false;
            }
            break;
        }
        case DefinitionNode::Variation::VARIANT:
            break;
    }
    return true;
}

bool Analyzer::analyze_scope(const Context &ctx, const Scope *scope) {
    for (const auto &statement : scope->body) {
        if (!analyze_statement(ctx, statement.get())) {
            return false;
        }
    }
    return true;
}

bool Analyzer::analyze_statement(const Context &ctx, const StatementNode *statement) {
    switch (statement->get_variation()) {
        case StatementNode::Variation::UNKNOWN_VARIATION:
            assert(false && "Unknown statement node type");
            return false;
        case StatementNode::Variation::ARRAY_ASSIGNMENT: {
            const auto *node = statement->as<ArrayAssignmentNode>();
            for (const auto &index_expr : node->indexing_expressions) {
                if (!analyze_expression(ctx, index_expr.get())) {
                    return false;
                }
            }
            if (!analyze_expression(ctx, node->expression.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::ASSIGNMENT: {
            const auto *node = statement->as<AssignmentNode>();
            if (!analyze_expression(ctx, node->expression.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::BREAK:
            break;
        case StatementNode::Variation::CALL: {
            const auto *node = statement->as<CallNodeStatement>();
            for (const auto &arg : node->arguments) {
                Context local_ctx = ctx;
                local_ctx.is_extern = !(node->function_name.size() > 3 && node->function_name.substr(0, 3) != "fc_");
                if (!analyze_expression(local_ctx, arg.first.get())) {
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::CATCH: {
            const auto *node = statement->as<CatchNode>();
            if (!analyze_scope(ctx, node->scope.get())) {
                return false;
            }
            // Analyze the call node inside catch
            for (const auto &arg : node->call_node->arguments) {
                if (!analyze_expression(ctx, arg.first.get())) {
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::CONTINUE:
            break;
        case StatementNode::Variation::DATA_FIELD_ASSIGNMENT: {
            const auto *node = statement->as<DataFieldAssignmentNode>();
            if (!analyze_expression(ctx, node->expression.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::DECLARATION: {
            const auto *node = statement->as<DeclarationNode>();
            if (node->initializer.has_value()) {
                if (!analyze_expression(ctx, node->initializer.value().get())) {
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::ENHANCED_FOR_LOOP: {
            const auto *node = statement->as<EnhForLoopNode>();
            if (!analyze_expression(ctx, node->iterable.get())) {
                return false;
            }
            if (!analyze_scope(ctx, node->body.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::FOR_LOOP: {
            const auto *node = statement->as<ForLoopNode>();
            if (!analyze_scope(ctx, node->definition_scope.get())) {
                return false;
            }
            if (!analyze_expression(ctx, node->condition.get())) {
                return false;
            }
            if (!analyze_statement(ctx, node->looparound.get())) {
                return false;
            }
            if (!analyze_scope(ctx, node->body.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::GROUP_ASSIGNMENT: {
            const auto *node = statement->as<GroupAssignmentNode>();
            if (!analyze_expression(ctx, node->expression.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::GROUP_DECLARATION: {
            const auto *node = statement->as<GroupDeclarationNode>();
            if (!analyze_expression(ctx, node->initializer.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::GROUPED_DATA_FIELD_ASSIGNMENT: {
            const auto *node = statement->as<GroupedDataFieldAssignmentNode>();
            if (!analyze_expression(ctx, node->expression.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::IF: {
            const auto *node = statement->as<IfNode>();
            if (!analyze_expression(ctx, node->condition.get())) {
                return false;
            }
            if (!analyze_scope(ctx, node->then_scope.get())) {
                return false;
            }
            if (node->else_scope.has_value()) {
                if (std::holds_alternative<std::shared_ptr<Scope>>(node->else_scope.value())) {
                    if (!analyze_scope(ctx, std::get<std::shared_ptr<Scope>>(node->else_scope.value()).get())) {
                        return false;
                    }
                } else {
                    if (!analyze_statement(ctx, std::get<std::unique_ptr<IfNode>>(node->else_scope.value()).get())) {
                        return false;
                    }
                }
            }
            break;
        }
        case StatementNode::Variation::RETURN: {
            const auto *node = statement->as<ReturnNode>();
            if (node->return_value.has_value()) {
                if (!analyze_expression(ctx, node->return_value.value().get())) {
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::STACKED_ASSIGNMENT: {
            const auto *node = statement->as<StackedAssignmentNode>();
            if (!analyze_expression(ctx, node->base_expression.get())) {
                return false;
            }
            if (!analyze_expression(ctx, node->expression.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::STACKED_GROUPED_ASSIGNMENT: {
            const auto *node = statement->as<StackedGroupedAssignmentNode>();
            if (!analyze_expression(ctx, node->base_expression.get())) {
                return false;
            }
            if (!analyze_expression(ctx, node->expression.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::SWITCH: {
            const auto *node = statement->as<SwitchStatement>();
            if (!analyze_expression(ctx, node->switcher.get())) {
                return false;
            }
            for (const auto &branch : node->branches) {
                for (const auto &match : branch.matches) {
                    if (!analyze_expression(ctx, match.get())) {
                        return false;
                    }
                }
                if (!analyze_scope(ctx, branch.body.get())) {
                    return false;
                }
            }
            break;
        }
        case StatementNode::Variation::THROW: {
            const auto *node = statement->as<ThrowNode>();
            if (!analyze_expression(ctx, node->throw_value.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::UNARY_OP: {
            const auto *node = statement->as<UnaryOpStatement>();
            if (!analyze_expression(ctx, node->operand.get())) {
                return false;
            }
            break;
        }
        case StatementNode::Variation::WHILE: {
            const auto *node = statement->as<WhileNode>();
            if (!analyze_expression(ctx, node->condition.get())) {
                return false;
            }
            if (!analyze_scope(ctx, node->scope.get())) {
                return false;
            }
            break;
        }
    }
    return true;
}

bool Analyzer::analyze_expression(const Context &ctx, const ExpressionNode *expression) {
    // Check if the type is a pointer type and if we are not in a context which allows pointer types
    if (dynamic_cast<const PointerType *>(expression->type.get())) {
        if (!ctx.is_extern) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }

    switch (expression->get_variation()) {
        case ExpressionNode::Variation::UNKNOWN_VARIATION:
            assert(false && "Unknown expression node type");
            return false;
        case ExpressionNode::Variation::ARRAY_ACCESS: {
            const auto *node = expression->as<ArrayAccessNode>();
            if (!analyze_expression(ctx, node->base_expr.get())) {
                return false;
            }
            for (const auto &index_expr : node->indexing_expressions) {
                if (!analyze_expression(ctx, index_expr.get())) {
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::ARRAY_INITIALIZER: {
            const auto *node = expression->as<ArrayInitializerNode>();
            for (const auto &length_expr : node->length_expressions) {
                if (!analyze_expression(ctx, length_expr.get())) {
                    return false;
                }
            }
            if (!analyze_expression(ctx, node->initializer_value.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::BINARY_OP: {
            const auto *node = expression->as<BinaryOpNode>();
            if (!analyze_expression(ctx, node->left.get())) {
                return false;
            }
            if (!analyze_expression(ctx, node->right.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::CALL: {
            const auto *node = expression->as<CallNodeExpression>();
            Context local_ctx = ctx;
            local_ctx.is_extern = !(node->function_name.size() > 3 && node->function_name.substr(0, 3) != "fc_");
            for (const auto &arg : node->arguments) {
                if (!analyze_expression(ctx, arg.first.get())) {
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::DATA_ACCESS: {
            const auto *node = expression->as<DataAccessNode>();
            if (!analyze_expression(ctx, node->base_expr.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::DEFAULT:
            break;
        case ExpressionNode::Variation::GROUP_EXPRESSION: {
            const auto *node = expression->as<GroupExpressionNode>();
            for (const auto &expr : node->expressions) {
                if (!analyze_expression(ctx, expr.get())) {
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::GROUPED_DATA_ACCESS: {
            const auto *node = expression->as<GroupedDataAccessNode>();
            if (!analyze_expression(ctx, node->base_expr.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::INITIALIZER: {
            const auto *node = expression->as<InitializerNode>();
            for (const auto &arg : node->args) {
                if (!analyze_expression(ctx, arg.get())) {
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::LITERAL:
            break;
        case ExpressionNode::Variation::OPTIONAL_CHAIN: {
            const auto *node = expression->as<OptionalChainNode>();
            if (!analyze_expression(ctx, node->base_expr.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::OPTIONAL_UNWRAP: {
            const auto *node = expression->as<OptionalUnwrapNode>();
            if (!analyze_expression(ctx, node->base_expr.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::RANGE_EXPRESSION: {
            const auto *node = expression->as<RangeExpressionNode>();
            if (!analyze_expression(ctx, node->lower_bound.get())) {
                return false;
            }
            if (!analyze_expression(ctx, node->upper_bound.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::STRING_INTERPOLATION: {
            const auto *node = expression->as<StringInterpolationNode>();
            for (const auto &content : node->string_content) {
                if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(content)) {
                    if (!analyze_expression(ctx, std::get<std::unique_ptr<ExpressionNode>>(content).get())) {
                        return false;
                    }
                }
            }
            break;
        }
        case ExpressionNode::Variation::SWITCH_EXPRESSION: {
            const auto *node = expression->as<SwitchExpression>();
            if (!analyze_expression(ctx, node->switcher.get())) {
                return false;
            }
            for (const auto &branch : node->branches) {
                if (!analyze_expression(ctx, branch.expr.get())) {
                    return false;
                }
                for (const auto &match : branch.matches) {
                    if (!analyze_expression(ctx, match.get())) {
                        return false;
                    }
                }
            }
            break;
        }
        case ExpressionNode::Variation::SWITCH_MATCH:
            break;
        case ExpressionNode::Variation::TYPE_CAST: {
            const auto *node = expression->as<TypeCastNode>();
            if (!analyze_expression(ctx, node->expr.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::TYPE:
            break;
        case ExpressionNode::Variation::UNARY_OP: {
            const auto *node = expression->as<UnaryOpExpression>();
            if (!analyze_expression(ctx, node->operand.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::VARIABLE:
            break;
        case ExpressionNode::Variation::VARIANT_EXTRACTION: {
            const auto *node = expression->as<VariantExtractionNode>();
            if (!analyze_expression(ctx, node->base_expr.get())) {
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::VARIANT_UNWRAP: {
            const auto *node = expression->as<VariantUnwrapNode>();
            if (!analyze_expression(ctx, node->base_expr.get())) {
                return false;
            }
            break;
        }
    }
    return true;
}
