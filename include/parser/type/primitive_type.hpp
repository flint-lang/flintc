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

    std::string to_string() const override {
        return type_name;
    }

    /// @var `type_name`
    /// @brief The name of the primitive type
    std::string type_name;
};
