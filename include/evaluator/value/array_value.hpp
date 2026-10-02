#pragma once

#include "parser/type/array_type.hpp"
#include "value.hpp"

#include <optional>

/// @class `ArrayValue`
/// @brief Represents compile-time array values
class ArrayValue : public Value {
  public:
    explicit ArrayValue(const std::shared_ptr<Value> &value, const std::shared_ptr<Type> &type) :
        Value(type) {
        ASSERT(type->get_variation() == Type::Variation::ARRAY);
        ASSERT(type->as<ArrayType>()->sizes.has_value());
        sizes = type->as<ArrayType>()->sizes.value();
        size_t total_size = 1;
        for (const auto &size : sizes) {
            total_size *= size;
        }
        data = std::vector<std::shared_ptr<Value>>(total_size, value);
    }

    explicit ArrayValue(const std::vector<std::shared_ptr<Value>> &data, const std::shared_ptr<Type> &type) :
        Value(type),
        data(data) {
        ASSERT(type->get_variation() == Type::Variation::ARRAY);
        ASSERT(type->as<ArrayType>()->sizes.has_value());
        sizes = type->as<ArrayType>()->sizes.value();
        size_t total_size = 1;
        for (const auto &size : sizes) {
            total_size *= size;
        }
        ASSERT(total_size == data.size());
    }

    Variation get_variation() const override {
        return Variation::ARRAY;
    }

    /// @function `get_value_at`
    /// @brief Returns a reference to the value stored at the given index
    ///
    /// @param `indices` The indices to get the value at (one index for every dimension)
    /// @return `std::optional<std::shared_ptr<Value> *>` A pointer to the element inside the array, nullopt if OOB access
    std::optional<std::shared_ptr<Value> *> get_value_at(const std::vector<size_t> &indices) {
        ASSERT(!indices.empty());
        ASSERT(indices.size() == sizes.size());
        for (size_t i = 0; i < indices.size(); i++) {
            if (indices.at(i) >= sizes.at(i)) {
                return std::nullopt;
            }
        }
        size_t address = indices.front();
        for (size_t i = 1; i < indices.size(); i++) {
            size_t chunk_size = 1;
            for (size_t j = 0; j < i; j++) {
                chunk_size *= sizes.at(j);
            }
            address += chunk_size * indices.at(i);
        }
        ASSERT(address < data.size());
        return &data.at(address);
    }

    /// @var `sizes`
    /// @brief The sizes of each dimension of the array. The length of this vector is the dimensionality of the array
    std::vector<size_t> sizes;

    /// @var `data`
    /// @brief The sequential data of the array
    std::vector<std::shared_ptr<Value>> data;
};
