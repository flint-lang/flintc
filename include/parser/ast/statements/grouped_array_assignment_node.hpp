#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// @class `GroupedArrayAssignmentNode`
/// @brief Represents grouped array assignments
class GroupedArrayAssignmentNode : public StatementNode {
  public:
    GroupedArrayAssignmentNode(                                             //
        std::unique_ptr<ExpressionNode> &base_expr,                         //
        std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions, //
        std::unique_ptr<ExpressionNode> &expression                         //
        ) :
        base_expr(std::move(base_expr)),
        indexing_expressions(std::move(indexing_expressions)),
        expression(std::move(expression)) {}

    Variation get_variation() const override {
        return Variation::GROUPED_ARRAY_ASSIGNMENT;
    }

    // constructor
    GroupedArrayAssignmentNode() = default;
    // deconstructor
    ~GroupedArrayAssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    GroupedArrayAssignmentNode(const GroupedArrayAssignmentNode &) = delete;
    GroupedArrayAssignmentNode &operator=(const GroupedArrayAssignmentNode &) = delete;
    // move operations
    GroupedArrayAssignmentNode(GroupedArrayAssignmentNode &&) = default;
    GroupedArrayAssignmentNode &operator=(GroupedArrayAssignmentNode &&) = default;

    /// @var `base_expr`
    /// @brief The base array expression on which to assign the value to
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `indexing_expressions`
    /// @brief The expressions of all the positions to insert, where each indexing expression is a group of N values, where N is the
    /// dimensionality of the array
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
