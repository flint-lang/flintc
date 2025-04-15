#pragma once

#include "type.hpp"

#include <memory>
#include <optional>

/// @class `ArrayType`
/// @brief Represents array types
class ArrayType : public Type {
  public:
    ArrayType(const std::optional<size_t> &length, const std::shared_ptr<Type> &type) :
        length(length),
        type(type) {}

    std::string to_string() override {
        return type->to_string() + "[]";
    }

    /// @var `length`
    /// @brief The length of the array type, nullopt if the length is unknown
    std::optional<size_t> length;

    /// @var `type`
    /// @brief The actual type of every array element
    std::shared_ptr<Type> type;
};
