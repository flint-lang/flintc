#pragma once

#include "type.hpp"

#include <memory>

/// @class `OptionalType`
/// @brief Represents optional types
class OptionalType : public Type {
  public:
    OptionalType(const std::shared_ptr<Type> &base_type) :
        base_type(base_type) {}

    Variation get_variation() const override {
        return Variation::OPTIONAL;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::OPTIONAL) {
            return false;
        }
        const OptionalType *const other_type = other->as<OptionalType>();
        return base_type->equals(other_type->base_type);
    }

    std::string to_string() const override {
        return base_type->to_string() + "?";
    }

    /// @var `base_type`
    /// @brief The actual base type of the optional type
    std::shared_ptr<Type> base_type;
};
