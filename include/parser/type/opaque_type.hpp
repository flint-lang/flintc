#pragma once

#include "globals.hpp"
#include "parser/hash.hpp"
#include "type.hpp"

#include <memory>

/// @class `OpaqueType`
/// @brief Represents opaque types
class OpaqueType : public Type {
  public:
    explicit OpaqueType(const std::string &name, const Hash &hash) :
        name(name),
        hash(hash) {}

    explicit OpaqueType() :
        name(std::nullopt),
        hash(Hash(std::string(""))) {}

    Variation get_variation() const override {
        return Variation::OPAQUE;
    }

    bool is_freeable() const override {
        // Any opaque is "freeable" because of leak-detection logic. The value itself actually is *not* freed
        // If the leak mode is silent then the code path for leak detection is never emitted at all
        return opaque_leak_mode != OpaqueLeakMode::SILENT;
    }

    bool is_dima_managed() const override {
        return false;
    }

    Hash get_hash() const override {
        return hash;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::OPAQUE) {
            return false;
        }
        const OpaqueType *const other_type = other->as<OpaqueType>();
        return name == other_type->name;
    }

    std::string to_string() const override {
        return name.has_value() ? name.value() : "opaque";
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret." : "type.";
        return type_str + "opaque";
    }

    /// @var `name`
    /// @brief Optional name for named opaques
    std::optional<std::string> name;

    /// @var `hash`
    /// @brief The file the opaque type was defined in, if the opaque is not named the hash is empty
    Hash hash;
};
