#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>

/// @class `GroupedDataFieldAssignmentNode`
/// @brief Represents assignments to a single data field
class GroupedDataFieldAssignmentNode : public StatementNode {
  public:
    GroupedDataFieldAssignmentNode(                            //
        std::unique_ptr<ExpressionNode> &base_expr,            //
        const std::vector<std::string> &field_names,           //
        const std::vector<unsigned int> &field_ids,            //
        const std::vector<std::shared_ptr<Type>> &field_types, //
        std::unique_ptr<ExpressionNode> &expression            //
        ) :
        base_expr(std::move(base_expr)),
        field_names(field_names),
        field_ids(field_ids),
        field_types(field_types),
        expression(std::move(expression)) {}

    Variation get_variation() const override {
        return Variation::GROUPED_DATA_FIELD_ASSIGNMENT;
    }

    // constructor
    GroupedDataFieldAssignmentNode() = default;
    // deconstructor
    ~GroupedDataFieldAssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    GroupedDataFieldAssignmentNode(const GroupedDataFieldAssignmentNode &) = delete;
    GroupedDataFieldAssignmentNode &operator=(const GroupedDataFieldAssignmentNode &) = delete;
    // move operations
    GroupedDataFieldAssignmentNode(GroupedDataFieldAssignmentNode &&) = default;
    GroupedDataFieldAssignmentNode &operator=(GroupedDataFieldAssignmentNode &&) = default;

    /// @var `base_expr`
    /// @brief The base expression expression of the grouped field assignment
    std::unique_ptr<ExpressionNode> base_expr;

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
