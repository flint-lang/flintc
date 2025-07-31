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

    std::string to_string() const override {
        const std::string base_type_str = base_type->to_string();
        if (base_type_str == "bool") {
            return "bool8";
        }
        return base_type_str + "x" + std::to_string(width);
    }

    /// @var `base_type`
    /// @brief The base type of this multi-type
    std::shared_ptr<Type> base_type;

    /// @var `width`
    /// @brief The width of the multi-type
    unsigned int width;
};
