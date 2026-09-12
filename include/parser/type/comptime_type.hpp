#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

/// @class `ComptimeType`
/// @brief Represents compile-time types in the bodies of definitions which wait to be resolved. For example in the comptime parameter `type
/// T` the type `T` becomes a comtpime-type in the body where the CPL is defined at
class ComptimeType : public Type {
  public:
    ComptimeType(const std::string &name) :
        name(name) {}

    Variation get_variation() const override {
        return Variation::COMPTIME;
    }

    bool is_freeable() const override {
        return false;
    }

    bool is_dima_managed() const override {
        return false;
    }

    bool is_default_constructible() const override {
        return false;
    }

    bool is_runtime_compatible() const override {
        return false;
    }

    std::optional<std::unique_ptr<ExpressionNode>> get_default_value( //
        const std::shared_ptr<Type> &self,                            //
        [[maybe_unused]] const Hash &hash,                            //
        [[maybe_unused]] const PosTriple &pos,                        //
        [[maybe_unused]] const unsigned int scope_id                  //
    ) const override {
        ASSERT(self.get() == static_cast<const Type *>(this));
        return std::nullopt;
    }

    Hash get_hash() const override {
        return Hash(std::string(""));
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::COMPTIME) {
            return false;
        }
        const ComptimeType *const other_type = other->as<ComptimeType>();
        return name == other_type->name;
    }

    std::string to_string() const override {
        return name;
    }

    std::string get_type_string([[maybe_unused]] const bool is_return_type = false) const override {
        ASSERT(false, "Comptime types cannot be used in the generator and already should have been resolved by now");
        UNREACHABLE();
    }

    /// @var `name`
    /// The name of the comptime parameter used to substitute this comptime type
    std::string name;
};
