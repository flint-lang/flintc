#pragma once

#include "expression_node.hpp"

#include <string>

/// @class `GroupedDataAccessNode`
/// @brief Represents the accessing of multiple data fields at once
class GroupedDataAccessNode : public ExpressionNode {
  public:
    GroupedDataAccessNode(                             //
        const std::shared_ptr<Type> &data_type,        //
        const std::string &var_name,                   //
        const std::vector<std::string> &field_names,   //
        const std::vector<unsigned int> &field_ids,    //
        const std::vector<std::shared_ptr<Type>> &type //
        ) :
        data_type(data_type),
        var_name(var_name),
        field_names(field_names),
        field_ids(field_ids) {
        this->type = type;
    }

    /// @var `data_type`
    /// @brief The type of the data the accessed variable has
    std::shared_ptr<Type> data_type;

    /// @var `var_name`
    /// @brief The name of the data variable
    std::string var_name;

    /// @var `field_names`
    /// @brief The name of the accessed fields
    std::vector<std::string> field_names;

    /// @var `field_ids`
    /// @brief The indexes of the fields in the data
    std::vector<unsigned int> field_ids;
};
