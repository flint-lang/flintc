#pragma once

#include "value.hpp"

/// @class `BoolValue`
/// @brief Represents compile-time known boolean values
class BoolValue : public Value {
  public:
    explicit BoolValue(const bool value) :
        Value(Type::get_primitive_type("bool")),
        value(value) {}

    Variation get_variation() const override {
        return Variation::BOOL;
    }

    /// @var `value`
    /// @brief The compile-time boolean value
    bool value;
};
