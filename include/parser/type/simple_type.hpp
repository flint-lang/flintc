#pragma once

#include "type.hpp"

/// @class `SimpleType`
/// @brief Represents simple types
class SimpleType : public Type {
  public:
    SimpleType(const std::string &type_name) :
        type_name(type_name) {}

    std::string to_string() override {
        return type_name;
    }

    /// @var `type_name`
    /// @brief The name of the simple type
    std::string type_name;
};
