#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

/// @class `TypeType`
/// @brief Represents the `type` type. This type is a pure compile-time type and can resolve to every other type
class TypeType : public Type {
  public:
    TypeType() = default;

    Variation get_variation() const override {
        return Variation::TYPE;
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
        return other->get_variation() == Variation::TYPE;
    }

    std::string to_string() const override {
        return "type";
    }

    std::string get_type_string([[maybe_unused]] const bool is_return_type = false) const override {
        ASSERT(false, "'type' types cannot be used in the generator and already should have been resolved by now");
        UNREACHABLE();
    }
};
