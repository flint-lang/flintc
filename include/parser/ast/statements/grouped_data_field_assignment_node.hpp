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
        const std::shared_ptr<Type> &data_type,                //
        const std::string &var_name,                           //
        const std::vector<std::string> &field_names,           //
        const std::vector<unsigned int> &field_ids,            //
        const std::vector<std::shared_ptr<Type>> &field_types, //
        std::unique_ptr<ExpressionNode> &expression            //
        ) :
        data_type(data_type),
        var_name(var_name),
        field_names(field_names),
        field_ids(field_ids),
        field_types(field_types),
        expression(std::move(expression)) {}

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

    /// @var `data_type`
    /// @brief The type of the data variable where the values are assigned ti
    std::shared_ptr<Type> data_type;

    /// @var `var_name`
    /// @brief The variable on which the fields are assigned
    std::string var_name;

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
