#pragma once

#include "parser/ast/expressions/literal_node.hpp"
#include "parser/hash.hpp"
#include "type.hpp"

/// @class `PrimitiveType`
/// @brief Represents primitive types
class PrimitiveType : public Type {
  public:
    PrimitiveType(const std::string &type_name) :
        type_name(type_name) {}

    Variation get_variation() const override {
        return Variation::PRIMITIVE;
    }

    bool is_freeable() const override {
        return type_name == "str";
    }

    bool is_dima_managed() const override {
        return false;
    }

    bool is_default_constructible() const override {
        return type_name != "int" && type_name != "float" && //
            (type_name == "str" || type_name == "bool" || type_name[0] == 'f' || type_name[0] == 'i' || type_name[0] == 'u');
    }

    std::optional<std::unique_ptr<ExpressionNode>> get_default_value( //
        const std::shared_ptr<Type> &self,                            //
        const Hash &hash,                                             //
        const PosTriple &pos,                                         //
        [[maybe_unused]] const unsigned int scope_id                  //
    ) const override {
        ASSERT(self.get() == static_cast<const Type *>(this));
        if (!is_default_constructible()) {
            return std::nullopt;
        }
        LitValue value;
        if (type_name == "str") {
            value = LitStr("");
        } else if (type_name == "bool") {
            value = LitBool(false);
        } else if (type_name[0] == 'f') {
            value = LitFloat(APFloat("0"));
        } else if (type_name[0] == 'i' || type_name[0] == 'u') {
            value = LitInt(APInt("0"));
        } else {
            UNREACHABLE();
        }
        return std::make_unique<LiteralNode>(hash, pos, value, self, false);
    }

    Hash get_hash() const override {
        return Hash(std::string(""));
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::PRIMITIVE) {
            return false;
        }
        const PrimitiveType *const other_type = other->as<PrimitiveType>();
        return type_name == other_type->type_name;
    }

    std::string to_string() const override {
        return type_name;
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? "type.ret." : "type.";
        return type_str + type_name;
    }

    /// @var `type_name`
    /// @brief The name of the primitive type
    std::string type_name;
};
