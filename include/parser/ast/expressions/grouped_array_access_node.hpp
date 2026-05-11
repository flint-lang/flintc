#pragma once

#include "expression_node.hpp"

#include <memory>
#include <vector>

/// @class `GroupedArrayAccessNode`
/// @brief Represents grouped array accesses
class GroupedArrayAccessNode : public ExpressionNode {
  public:
    GroupedArrayAccessNode(                                                //
        std::unique_ptr<ExpressionNode> &base_expr,                        //
        std::shared_ptr<Type> result_type,                                 //
        std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
        ) :
        ExpressionNode(base_expr->is_const),
        base_expr(std::move(base_expr)),
        indexing_expressions(std::move(indexing_expressions)) {
        this->type = result_type;
    }

    Variation get_variation() const override {
        return Variation::GROUPED_ARRAY_ACCESS;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::vector<std::unique_ptr<ExpressionNode>> cloned_indexing_exprs;
        for (auto &expr : indexing_expressions) {
            cloned_indexing_exprs.emplace_back(expr->clone(scope_id));
        }
        std::unique_ptr<ExpressionNode> base_expr_clone = base_expr->clone(scope_id);
        return std::make_unique<GroupedArrayAccessNode>(base_expr_clone, this->type, cloned_indexing_exprs);
    }

    /// @var `base_expr`
    /// @brief The base expression from which the array elements are accessed
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `indexing_expressions`
    /// @brief The expressions of all the positions to extract, where each indexing expression is a group of N values, where N is the
    /// dimensionality of the array
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;
};
