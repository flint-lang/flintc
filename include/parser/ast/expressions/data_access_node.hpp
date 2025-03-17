#pragma once

#include "expression_node.hpp"

#include <string>

/// @class `DataAccessNode`
/// @brief Represents the accessing of a single datas value
class DataAccessNode : public ExpressionNode {
  public:
    DataAccessNode(const std::string &var_name, const std::string &field_name, const unsigned int field_id, const std::string &type) :
        var_name(var_name),
        field_name(field_name),
        field_id(field_id) {
        this->type = type;
    }

    /// @var `data_name`
    /// @brief The name of the data variable
    std::string var_name;

    /// @var `field_name`
    /// @brief The name of the accessed field
    std::string field_name;

    /// @var `field_id`
    /// @brief The index of the field in the data
    unsigned int field_id;
};
