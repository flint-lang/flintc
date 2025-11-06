#pragma once

#include "type.hpp"

#include <memory>

/// @class `PointerType`
/// @brief Represents pointer types
class PointerType : public Type {
  public:
    PointerType(const std::shared_ptr<Type> &base_type) :
        base_type(base_type) {}

    std::string to_string() const override {
        return base_type->to_string() + "*";
    }

    /// @var `base_type`
    /// @brief The actual base type of the pointer type
    std::shared_ptr<Type> base_type;
};
