#pragma once

#include "parser/hash.hpp"
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

    bool is_freeable() const override {
        return true;
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
        return type->equals(other_type->type);
    }

    std::string to_string() const override {
        return type->to_string() + "[" + std::string(dimensionality - 1, ',') + "]";
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret.str" : "type.str";
        return type_str;
    }

    /// @var `dimensionality`
    /// @brief The dimensionality of the array
    size_t dimensionality;

    /// @var `type`
    /// @brief The actual type of every array element
    std::shared_ptr<Type> type;
};
