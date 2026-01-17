#pragma once

#include "type.hpp"

#include <memory>

/// @class `PointerType`
/// @brief Represents pointer types
class PointerType : public Type {
  public:
    PointerType(const std::shared_ptr<Type> &base_type) :
        base_type(base_type) {}

    Variation get_variation() const override {
        return Variation::POINTER;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::POINTER) {
            return false;
        }
        const PointerType *const other_type = other->as<PointerType>();
        return base_type->equals(other_type->base_type);
    }

    std::string to_string() const override {
        return base_type->to_string() + "*";
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret." : "type.";
        return type_str + to_string();
    }

    /// @var `base_type`
    /// @brief The actual base type of the pointer type
    std::shared_ptr<Type> base_type;
};
