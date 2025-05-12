#pragma once

#include "expression_node.hpp"

#include <string>
#include <variant>

/// @class `DataAccessNode`
/// @brief Represents the accessing of a single datas value
class DataAccessNode : public ExpressionNode {
  public:
    DataAccessNode(                                                           //
        const std::shared_ptr<Type> &data_type,                               //
        std::variant<std::string, std::unique_ptr<ExpressionNode>> &variable, //
        const std::string &field_name,                                        //
        const unsigned int field_id,                                          //
        const std::shared_ptr<Type> &field_type                               //
        ) :
        data_type(data_type),
        variable(std::move(variable)),
        field_name(field_name),
        field_id(field_id) {
        this->type = field_type;
    }

    /// @var `data_type`
    /// @brief The type of the data variable
    std::shared_ptr<Type> data_type;

    /// @var `variable`
    /// @brief Either the name of the data variable for direct data accesses or the lhs expression for stacked accesses
    std::variant<std::string, std::unique_ptr<ExpressionNode>> variable;

    /// @var `field_name`
    /// @brief The name of the accessed field
    std::string field_name;

    /// @var `field_id`
    /// @brief The index of the field in the data
    unsigned int field_id;
};
