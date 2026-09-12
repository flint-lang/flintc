#pragma once

#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/hash.hpp"
#include "type.hpp"

#include <memory>

/// @class `OptionalType`
/// @brief Represents optional types
class OptionalType : public Type {
  public:
    OptionalType(const std::shared_ptr<Type> &base_type) :
        base_type(base_type) {}

    Variation get_variation() const override {
        return Variation::OPTIONAL;
    }

    bool is_freeable() const override {
        return base_type->is_freeable();
    }

    bool is_dima_managed() const override {
        return false;
    }

    bool is_default_constructible() const override {
        return true;
    }

    bool is_runtime_compatible() const override {
        return base_type->is_runtime_compatible();
    }

    std::optional<std::unique_ptr<ExpressionNode>> get_default_value( //
        const std::shared_ptr<Type> &self,                            //
        const Hash &hash,                                             //
        const PosTriple &pos,                                         //
        [[maybe_unused]] const unsigned int scope_id                  //
    ) const override {
        ASSERT(self.get() == static_cast<const Type *>(this));
        LitValue value = LitOptional();
        std::unique_ptr<ExpressionNode> literal = std::make_unique<LiteralNode>(hash, pos, value, Type::get_primitive_type("void?"), false);
        return std::make_unique<TypeCastNode>(hash, pos, self, literal);
    }

    Hash get_hash() const override {
        return base_type->get_hash();
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::OPTIONAL) {
            return false;
        }
        const OptionalType *const other_type = other->as<OptionalType>();
        return base_type->equals(other_type->base_type);
    }

    std::string to_string() const override {
        return base_type->to_string() + "?";
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret.optional." : "type.optional.";
        return type_str + to_string();
    }

    /// @var `base_type`
    /// @brief The actual base type of the optional type
    std::shared_ptr<Type> base_type;
};
