#pragma once

#include "value.hpp"

/// @class `CharValue`
/// @brief Represents compile-time known character values
class CharValue : public Value {
  public:
    explicit CharValue(const char value) :
        Value(Type::get_primitive_type("u8")),
        value(value) {}

    Variation get_variation() const override {
        return Variation::CHAR;
    }

    /// @var `value`
    /// @brief The compile-time character value
    char value;
};
