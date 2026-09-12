#pragma once

#include "parser/ast/definitions/object_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/namespace.hpp"
#include "type.hpp"

/// @class `ObjectType`
/// @brief Represents object types
class ObjectType : public Type {
  public:
    ObjectType(ObjectNode *const object_node) :
        object_node(object_node) {}

    Variation get_variation() const override {
        return Variation::OBJECT;
    }

    bool is_freeable() const override {
        return true;
    }

    bool is_dima_managed() const override {
        return true;
    }

    bool is_default_constructible() const override {
        for (const auto &[data_node, accessor] : object_node->data_components) {
            const auto &type = object_node->file_hash.get_namespace()->get_type_from_ptr(data_node);
            if (!type.has_value() || !type.value()->is_default_constructible()) {
                return false;
            }
        }
        return true;
    }

    bool is_runtime_compatible() const override {
        return object_node->cpl.empty();
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
        for (const auto &[data_node, accessor] : object_node->data_components) {
            const auto type = object_node->file_hash.get_namespace()->get_type_from_ptr(data_node).value();
            fields.emplace_back(InitializerNode::Field{
                .name = accessor,
                .value = type->get_default_value(type, hash, pos, scope_id).value(),
            });
        }
        return std::make_unique<InitializerNode>(hash, pos, self, fields);
    }

    Hash get_hash() const override {
        return object_node->file_hash;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::OBJECT) {
            return false;
        }
        const ObjectType *const other_type = other->as<ObjectType>();
        return object_node == other_type->object_node;
    }

    std::string to_string() const override {
        return object_node->name;
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? ".type.ret.object." : ".type.object.";
        return object_node->file_hash.to_string() + type_str + object_node->name;
    }

    /// @var `object_node`
    /// @brief The object node this object type points to
    ObjectNode *const object_node;
};
