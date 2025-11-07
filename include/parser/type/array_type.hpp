#pragma once

#include "type.hpp"

#include <memory>

/// @class `ArrayType`
/// @brief Represents array types
class ArrayType : public Type {
  public:
    ArrayType(const size_t dimensionality, const std::shared_ptr<Type> &type) :
        dimensionality(dimensionality),
        type(type) {}

    Variation get_variation() const override {
        return Variation::ARRAY;
    }

    std::string to_string() const override {
        return type->to_string() + "[" + std::string(dimensionality - 1, ',') + "]";
    }

    /// @var `dimensionality`
    /// @brief The dimensionality of the array
    size_t dimensionality;

    /// @var `type`
    /// @brief The actual type of every array element
    std::shared_ptr<Type> type;
};
