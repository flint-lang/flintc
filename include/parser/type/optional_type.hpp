#pragma once

#include "type.hpp"

#include <memory>

/// @class `OptionalType`
/// @brief Represents optional types
class OptionalType : public Type {
  public:
    OptionalType(const std::shared_ptr<Type> &base_type) :
        base_type(base_type) {}

    std::string to_string() override {
        return base_type->to_string() + "?";
    }

    /// @var `base_type`
    /// @brief The actual base type of the optional type
    std::shared_ptr<Type> base_type;
};
