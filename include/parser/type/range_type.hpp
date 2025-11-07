#pragma once

#include "parser/type/type.hpp"

/// @class `RangeType`
/// @brief Represents range types
class RangeType : public Type {
  public:
    RangeType(const std::shared_ptr<Type> &bound_type) :
        bound_type(bound_type) {}

    Variation get_variation() const override {
        return Variation::RANGE;
    }

    std::string to_string() const override {
        return "range<" + bound_type->to_string() + ">";
    }

    /// @var `bound_type`
    /// @brief The type of the range bounds
    std::shared_ptr<Type> bound_type;
};
