#pragma once

#include "expression_node.hpp"

#include <memory>

/// @class `TypeCastNode`
/// @brief Represents type casts
class TypeCastNode : public ExpressionNode {
  public:
    TypeCastNode(                             //
        const Hash &hash,                     //
        const PosTriple &pos,                 //
        const std::shared_ptr<Type> &type,    //
        std::unique_ptr<ExpressionNode> &expr //
        ) :
        ExpressionNode(hash, pos, expr->is_const),
        expr(std::move(expr)) {
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::TYPE_CAST;
    }

    bool is_literal() const override {
        return expr->is_literal();
    }

    bool is_comptime_known() const override {
        return expr->is_comptime_known();
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::unique_ptr<ExpressionNode> expr_clone = expr->clone(scope_id);
        return std::make_unique<TypeCastNode>(file_hash, PosTriple{line, column, length}, type, expr_clone);
    }

    /// @var `expr`
    /// @brief The expression of the type cast node
    std::unique_ptr<ExpressionNode> expr;
};
