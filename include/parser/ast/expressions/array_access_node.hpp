#pragma once

#include "expression_node.hpp"

#include <memory>
#include <vector>

/// @class `ArrayAccessNode`
/// @brief Represents array accesses
class ArrayAccessNode : public ExpressionNode {
  public:
    ArrayAccessNode(                                                       //
        std::unique_ptr<ExpressionNode> &base_expr,                        //
        std::shared_ptr<Type> result_type,                                 //
        std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
        ) :
        base_expr(std::move(base_expr)),
        indexing_expressions(std::move(indexing_expressions)) {
        this->type = result_type;
    }

    Variation get_variation() const override {
        return Variation::ARRAY_ACCESS;
    }

    std::unique_ptr<ExpressionNode> clone() const override {
        std::vector<std::unique_ptr<ExpressionNode>> cloned_indexing_exprs;
        for (auto &expr : indexing_expressions) {
            cloned_indexing_exprs.emplace_back(expr->clone());
        }
        std::unique_ptr<ExpressionNode> base_expr_clone = base_expr->clone();
        return std::make_unique<ArrayAccessNode>(base_expr_clone, this->type, cloned_indexing_exprs);
    }

    /// @var `base_expr`
    /// @brief The base expression from which the array elements are accessed
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `indexing_expressions`
    /// @brief The expressions of all the dimensions indices
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;
};
