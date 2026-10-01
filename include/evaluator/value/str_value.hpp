#pragma once

#include "value.hpp"

/// @class `StrValue`
/// @brief Represents compile-time known string values
class StrValue : public Value {
  public:
    explicit StrValue(const std::string &value) :
        Value(Type::get_primitive_type("type.flint.str.lit")),
        value(value) {}

    Variation get_variation() const override {
        return Variation::STR;
    }

    /// @var `value`
    /// @brief The compile-time string value
    std::string value;
};
