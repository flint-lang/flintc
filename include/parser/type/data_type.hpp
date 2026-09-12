#pragma once

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
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
        return !data_node->is_const && !data_node->is_shared;
    }

    bool is_dima_managed() const override {
        return true;
    }

    bool is_default_constructible() const override {
        for (const auto &field : data_node->fields) {
            if (!field.initializer.has_value() && !field.type->is_default_constructible()) {
                return false;
            }
        }
        return true;
    }

    bool is_runtime_compatible() const override {
        return data_node->cpl.empty();
    }

    std::optional<std::unique_ptr<ExpressionNode>> get_default_value( //
        const std::shared_ptr<Type> &self,                            //
        const Hash &hash,                                             //
        const PosTriple &pos,                                         //
        const unsigned int scope_id                                   //
    ) const override {
        ASSERT(self.get() == static_cast<const Type *>(this));
        if (!is_default_constructible()) {
            return std::nullopt;
        }
        std::vector<InitializerNode::Field> fields;
        for (const auto &field : data_node->fields) {
            fields.emplace_back(InitializerNode::Field{
                .name = field.name,
                .value = field.initializer.has_value()           //
                    ? field.initializer.value()->clone(scope_id) //
                    : field.type->get_default_value(field.type, hash, pos, scope_id).value(),
            });
        }
        return std::make_unique<InitializerNode>(hash, pos, self, fields);
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
