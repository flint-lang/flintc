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

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::RANGE) {
            return false;
        }
        const RangeType *const other_type = other->as<RangeType>();
        return bound_type->equals(other_type->bound_type);
    }

    std::string to_string() const override {
        return "range<" + bound_type->to_string() + ">";
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret." : "type.";
        return type_str + to_string();
    }

    /// @var `bound_type`
    /// @brief The type of the range bounds
    std::shared_ptr<Type> bound_type;
};
