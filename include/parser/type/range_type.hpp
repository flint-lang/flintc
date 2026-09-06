#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

/// @class `RangeType`
/// @brief Represents range types
class RangeType : public Type {
  public:
    RangeType(const std::shared_ptr<Type> &bound_type) :
        bound_type(bound_type) {}

    Variation get_variation() const override {
        return Variation::RANGE;
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
        if (other->get_variation() != Variation::RANGE) {
            return false;
        }
        const RangeType *const other_type = other->as<RangeType>();
        return bound_type->equals(other_type->bound_type);
    }

    std::string to_string() const override {
        return "range<" + bound_type->to_string() + ">";
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret." : "type.";
        return type_str + to_string();
    }

    /// @var `bound_type`
    /// @brief The type of the range bounds
    std::shared_ptr<Type> bound_type;
};
