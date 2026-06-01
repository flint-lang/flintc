#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

#include <memory>
#include <sstream>

/// @class `ArrayType`
/// @brief Represents array types
class ArrayType : public Type {
  public:
    ArrayType(const size_t dimensionality, const std::shared_ptr<Type> &type, const std::optional<std::vector<size_t>> &sizes) :
        dimensionality(dimensionality),
        type(type),
        sizes(sizes) {
        assert(!sizes.has_value() || dimensionality == sizes.value().size());
    }

    Variation get_variation() const override {
        return Variation::ARRAY;
    }

    bool is_freeable() const override {
        return true;
    }

    bool is_dima_managed() const override {
        return false;
    }

    Hash get_hash() const override {
        return type->get_hash();
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::ARRAY) {
            return false;
        }
        const ArrayType *other_type = other->as<ArrayType>();
        if (dimensionality != other_type->dimensionality) {
            return false;
        }
        if (sizes != other_type->sizes) {
            return false;
        }
        return type->equals(other_type->type);
    }

    std::string to_string() const override {
        if (!sizes.has_value()) {
            return type->to_string() + "[" + std::string(dimensionality - 1, ',') + "]";
        }
        assert(sizes.value().size() == dimensionality);
        std::stringstream type_str;
        type_str << type->to_string() << "[";
        for (size_t i = 0; i < dimensionality; i++) {
            if (i > 0) {
                type_str << ", ";
            }
            type_str << std::to_string(sizes.value().at(i));
        }
        type_str << "]";
        return type_str.str();
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        std::string type_str;
        if (is_return_type) {
            type_str = "type.ret.";
        } else {
            type_str = "type.";
        }
        if (sizes.has_value()) {
            type_str += "str";
        } else {
            type_str += to_string();
        }
        return type_str;
    }

    /// @var `dimensionality`
    /// @brief The dimensionality of the array
    size_t dimensionality;

    /// @var `type`
    /// @brief The actual type of every array element
    std::shared_ptr<Type> type;

    /// @var `sizes`
    /// @brief The sizes of each dimension of the array, these sizes need to be compile-time known, for example the `i32[3]` type has one
    /// size element, `3`
    std::optional<std::vector<size_t>> sizes;
};
