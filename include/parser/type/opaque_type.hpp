#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

#include <memory>

/// @class `OpaqueType`
/// @brief Represents opaque types
class OpaqueType : public Type {
  public:
    OpaqueType(const std::optional<std::string> &name) :
        name(name) {}

    Variation get_variation() const override {
        return Variation::OPAQUE;
    }

    bool is_freeable() const override {
        return false;
    }

    Hash get_hash() const override {
        return Hash(std::string(""));
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::OPAQUE) {
            return false;
        }
        const OpaqueType *const other_type = other->as<OpaqueType>();
        return name == other_type->name;
    }

    std::string to_string() const override {
        return name.has_value()              //
            ? "opaque<" + name.value() + ">" //
            : "opaque";
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret." : "type.";
        return type_str + "opaque";
    }

    /// @var `name`
    /// @brief Optional name for named opaques
    std::optional<std::string> name;
};
