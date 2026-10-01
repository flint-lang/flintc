#pragma once

#include "parser/ap_float.hpp"
#include "value.hpp"

/// @class `FloatValue`
/// @brief Represents compile-time floating point values
class FloatValue : public Value {
  public:
    explicit FloatValue(const APFloat &value) :
        Value(Type::get_primitive_type("float")),
        value(value) {}

    Variation get_variation() const override {
        return Variation::FLOAT;
    }

    /// @var `value`
    /// @brief The compile-time floating point value
    APFloat value;
};
