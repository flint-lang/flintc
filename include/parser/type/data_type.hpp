#pragma once

#include "parser/ast/definitions/data_node.hpp"
#include "type.hpp"

/// @class `DataType`
/// @brief Represents data types
class DataType : public Type {
  public:
    DataType(DataNode *const data_node) :
        data_node(data_node) {}

    Variation get_variation() const override {
        return Variation::DATA;
    }

    bool is_freeable() const override {
        return true;
    }

    Hash get_hash() const override {
        return data_node->file_hash;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::DATA) {
            return false;
        }
        const DataType *const other_type = other->as<DataType>();
        return data_node == other_type->data_node;
    }

    std::string to_string() const override {
        return data_node->name;
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? ".type.ret.data." : ".type.data.";
        return data_node->file_hash.to_string() + type_str + data_node->name;
    }

    /// @var `data_node`
    /// @brief The data node this data type points to
    DataNode *const data_node;
};
