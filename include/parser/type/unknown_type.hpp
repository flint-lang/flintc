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

    std::string to_string() const override {
        return "Unknown(" + type_str + ")";
    }

    /// @var `type_str`
    /// The string representation of the type
    std::string type_str;
};
