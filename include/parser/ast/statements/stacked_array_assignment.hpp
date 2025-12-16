#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// @class `StackedArrayAssignmentNode`
/// @brief Represents stacked array assignments
class StackedArrayAssignmentNode : public StatementNode {
  public:
    StackedArrayAssignmentNode(                                             //
        std::unique_ptr<ExpressionNode> &base_expression,                   //
        std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions, //
        std::unique_ptr<ExpressionNode> &expression                         //
        ) :
        base_expression(std::move(base_expression)),
        indexing_expressions(std::move(indexing_expressions)),
        expression(std::move(expression)) {}

    Variation get_variation() const override {
        return Variation::STACKED_ARRAY_ASSIGNMENT;
    }

    // constructor
    StackedArrayAssignmentNode() = delete;
    // deconstructor
    ~StackedArrayAssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    StackedArrayAssignmentNode(const StackedArrayAssignmentNode &) = delete;
    StackedArrayAssignmentNode &operator=(const StackedArrayAssignmentNode &) = delete;
    // move operations
    StackedArrayAssignmentNode(StackedArrayAssignmentNode &&) = default;
    StackedArrayAssignmentNode &operator=(StackedArrayAssignmentNode &&) = default;

    /// @var `base_expression`
    /// @brief The lhs expression at which the rhs expression is assigned to
    ///
    /// @note If the lhs is `var.field[idx] = ...` the `base_expression` is `var.field`.
    std::unique_ptr<ExpressionNode> base_expression;

    /// @var `indexing_expressions`
    /// @brief The expressions at which index to assign the expression to
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
