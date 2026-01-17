#pragma once

#include "type.hpp"

/// @class `UnknownType`
/// @brief Represents unknown types
class UnknownType : public Type {
  public:
    UnknownType(const std::string &type_str) :
        type_str(type_str) {}

    Variation get_variation() const override {
        return Variation::UNKNOWN;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::UNKNOWN) {
            return false;
        }
        const UnknownType *const other_type = other->as<UnknownType>();
        return type_str == other_type->type_str;
    }

    std::string to_string() const override {
        return "Unknown(" + type_str + ")";
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_string = is_return_type ? "type.ret." : "type.";
        return type_string + type_str;
    }

    /// @var `type_str`
    /// The string representation of the type
    std::string type_str;
};
