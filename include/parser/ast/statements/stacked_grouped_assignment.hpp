#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>

/// @class `StackedGroupedAssignmentNode`
/// @brief Represents stacked assignments to a multiple data field
class StackedGroupedAssignmentNode : public StatementNode {
  public:
    StackedGroupedAssignmentNode(                              //
        std::unique_ptr<ExpressionNode> &base_expression,      //
        const std::vector<std::string> &field_names,           //
        const std::vector<unsigned int> &field_ids,            //
        const std::vector<std::shared_ptr<Type>> &field_types, //
        std::unique_ptr<ExpressionNode> &expression            //
        ) :
        base_expression(std::move(base_expression)),
        field_names(field_names),
        field_ids(field_ids),
        field_types(field_types),
        expression(std::move(expression)) {}

    // constructor
    StackedGroupedAssignmentNode() = delete;
    // deconstructor
    ~StackedGroupedAssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    StackedGroupedAssignmentNode(const StackedGroupedAssignmentNode &) = delete;
    StackedGroupedAssignmentNode &operator=(const StackedGroupedAssignmentNode &) = delete;
    // move operations
    StackedGroupedAssignmentNode(StackedGroupedAssignmentNode &&) = default;
    StackedGroupedAssignmentNode &operator=(StackedGroupedAssignmentNode &&) = default;

    /// @var `base_expression`
    /// @brief The lhs expression at which the rhs expression is assigned to
    ///
    /// @note If the lhs is `var.field.field = ...` the `base_expression` is `var.field`.
    std::unique_ptr<ExpressionNode> base_expression;

    /// @var `field_names`
    /// @brief The names of the fields to assign to
    std::vector<std::string> field_names;

    /// @var `field_ids`
    /// @brief The ids of the fields to assign to
    std::vector<unsigned int> field_ids;

    /// @var `field_types`
    /// @brief The types of the fields to assign to
    std::vector<std::shared_ptr<Type>> field_types;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
