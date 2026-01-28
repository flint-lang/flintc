#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

#include <memory>

/// @class `AliasType`
/// @brief Represents type aliases
class AliasType : public Type {
  public:
    AliasType(const std::string &alias, const std::shared_ptr<Type> &type) :
        alias(alias),
        type(type) {}

    Variation get_variation() const override {
        return Variation::ALIAS;
    }

    bool is_freeable() const override {
        return type->is_freeable();
    }

    Hash get_hash() const override {
        return type->get_hash();
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::ALIAS) {
            return type->equals(other);
        }
        const AliasType *const other_type = other->as<AliasType>();
        return type->equals(other_type->type);
    }

    std::string to_string() const override {
        return alias;
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        return type->get_type_string(is_return_type);
    }

    /// @var `alias`
    /// @brief The alias of the type
    std::string alias;

    /// @var `type`
    /// @brief The actual type of the aliased type
    std::shared_ptr<Type> type;
};
