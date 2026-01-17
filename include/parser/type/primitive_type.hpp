#pragma once

#include "type.hpp"

/// @class `PrimitiveType`
/// @brief Represents primitive types
class PrimitiveType : public Type {
  public:
    PrimitiveType(const std::string &type_name) :
        type_name(type_name) {}

    Variation get_variation() const override {
        return Variation::PRIMITIVE;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::PRIMITIVE) {
            return false;
        }
        const PrimitiveType *const other_type = other->as<PrimitiveType>();
        return type_name == other_type->type_name;
    }

    std::string to_string() const override {
        return type_name;
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret." : "type.";
        return type_str + type_name;
    }

    /// @var `type_name`
    /// @brief The name of the primitive type
    std::string type_name;
};
