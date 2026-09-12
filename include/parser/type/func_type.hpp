#pragma once

#include "parser/ast/definitions/func_node.hpp"
#include "type.hpp"

/// @class `FuncType`
/// @brief Represents func types
class FuncType : public Type {
  public:
    FuncType(FuncNode *const func_node) :
        func_node(func_node) {}

    Variation get_variation() const override {
        return Variation::FUNC;
    }

    bool is_freeable() const override {
        return true;
    }

    bool is_dima_managed() const override {
        return false;
    }

    bool is_default_constructible() const override {
        return false;
    }

    bool is_runtime_compatible() const override {
        return func_node->cpl.empty();
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
        return func_node->file_hash;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::FUNC) {
            return false;
        }
        const FuncType *const other_type = other->as<FuncType>();
        return func_node == other_type->func_node;
    }

    std::string to_string() const override {
        return func_node->name;
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? ".type.ret.func." : ".type.func.";
        return func_node->file_hash.to_string() + type_str + func_node->name;
    }

    /// @var `func_node`
    /// @brief The func node this func type points to
    FuncNode *const func_node;
};
