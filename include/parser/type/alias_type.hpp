#pragma once

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

    /// @var `alias`
    /// @brief The alias of the type
    std::string alias;

    /// @var `type`
    /// @brief The actual type of the aliased type
    std::shared_ptr<Type> type;
};
