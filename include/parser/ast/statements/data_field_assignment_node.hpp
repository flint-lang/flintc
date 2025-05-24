#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>

/// @class `DataFieldAssignmentNode`
/// @brief Represents assignments to a single data field
class DataFieldAssignmentNode : public StatementNode {
  public:
    DataFieldAssignmentNode(                          //
        const std::shared_ptr<Type> &data_type,       //
        const std::string &var_name,                  //
        const std::optional<std::string> &field_name, //
        const unsigned int field_id,                  //
        const std::shared_ptr<Type> &field_type,      //
        std::unique_ptr<ExpressionNode> &expression   //
        ) :
        data_type(data_type),
        var_name(var_name),
        field_name(field_name),
        field_id(field_id),
        field_type(field_type),
        expression(std::move(expression)) {}

    // constructor
    DataFieldAssignmentNode() = default;
    // deconstructor
    ~DataFieldAssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    DataFieldAssignmentNode(const DataFieldAssignmentNode &) = delete;
    DataFieldAssignmentNode &operator=(const DataFieldAssignmentNode &) = delete;
    // move operations
    DataFieldAssignmentNode(DataFieldAssignmentNode &&) = default;
    DataFieldAssignmentNode &operator=(DataFieldAssignmentNode &&) = default;

    /// @var `data_type`
    /// @brief The type of the accessed data variable
    std::shared_ptr<Type> data_type;

    /// @var `var_name`
    /// @brief The variable on which the fields are assigned
    std::string var_name;

    /// @var `field_name`
    /// @brief The name of the field to assign to
    std::optional<std::string> field_name;

    /// @var `field_id`
    /// @brief The id of the field to assign to
    unsigned int field_id;

    /// @var `field_type`
    /// @brief The type of the field to assign to
    std::shared_ptr<Type> field_type;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
