#pragma once

#include "type.hpp"

#include <string>

/// @class `MultiType`
/// @brief Represents multi-types
class MultiType : public Type {
  public:
    MultiType(const std::shared_ptr<Type> &base_type, const unsigned int width) :
        base_type(base_type),
        width(width) {}

    Variation get_variation() const override {
        return Variation::MULTI;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::MULTI) {
            return false;
        }
        const MultiType *const other_type = other->as<MultiType>();
        if (width != other_type->width) {
            return false;
        }
        return base_type->equals(other_type->base_type);
    }

    std::string to_string() const override {
        const std::string base_type_str = base_type->to_string();
        if (base_type_str == "bool") {
            return "bool8";
        }
        return base_type_str + "x" + std::to_string(width);
    }

    std::string get_type_string([[maybe_unused]] const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret." : "type.";
        return type_str + to_string();
    }

    /// @var `base_type`
    /// @brief The base type of this multi-type
    std::shared_ptr<Type> base_type;

    /// @var `width`
    /// @brief The width of the multi-type
    unsigned int width;
};
