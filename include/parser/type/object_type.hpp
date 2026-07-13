#pragma once

#include "parser/ast/definitions/object_node.hpp"
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
