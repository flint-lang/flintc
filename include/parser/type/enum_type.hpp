#pragma once

#include "parser/ast/definitions/enum_node.hpp"
#include "type.hpp"

/// @class `EnumType`
/// @brief Represents enum types
class EnumType : public Type {
  public:
    EnumType(EnumNode *const enum_node) :
        enum_node(enum_node) {}

    Variation get_variation() const override {
        return Variation::ENUM;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::ENUM) {
            return false;
        }
        const EnumType *const other_type = other->as<EnumType>();
        return enum_node == other_type->enum_node;
    }

    std::string to_string() const override {
        return enum_node->name;
    }

    /// @var `enum_node`
    /// @brief The enum node this enum type points to
    EnumNode *const enum_node;
};
