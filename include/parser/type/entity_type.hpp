#pragma once

#include "parser/ast/definitions/entity_node.hpp"
#include "type.hpp"

/// @class `EntityType`
/// @brief Represents func types
class EntityType : public Type {
  public:
    EntityType(EntityNode *const entity_node) :
        entity_node(entity_node) {}

    Variation get_variation() const override {
        return Variation::ENTITY;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::ENTITY) {
            return false;
        }
        const EntityType *const other_type = other->as<EntityType>();
        return entity_node == other_type->entity_node;
    }

    std::string to_string() const override {
        return entity_node->name;
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? ".type.ret.entity." : ".type.entity.";
        return entity_node->file_hash.to_string() + type_str + entity_node->name;
    }

    /// @var `entity_node`
    /// @brief The entity node this entity type points to
    EntityNode *const entity_node;
};
