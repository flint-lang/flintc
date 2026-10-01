#pragma once

#include "parser/ap_int.hpp"
#include "value.hpp"

/// @class `IntValue`
/// @brief Represents compile-time integer values
class IntValue : public Value {
  public:
    explicit IntValue(const APInt &value) :
        Value(Type::get_primitive_type("int")),
        value(value) {}

    Variation get_variation() const override {
        return Variation::INT;
    }

    /// @var `value`
    /// @brief The compile-time integer value
    APInt value;
};
