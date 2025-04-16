#pragma once

#include "parser/ast/definitions/data_node.hpp"
#include "type.hpp"

/// @class `DataType`
/// @brief Represents data types
class DataType : public Type {
  public:
    DataType(DataNode *const data_node) :
        data_node(data_node) {}

    std::string to_string() override {
        return data_node->name;
    }

    /// @var `data_node`
    /// @brief The data node this data type points to
    DataNode *const data_node;
};
