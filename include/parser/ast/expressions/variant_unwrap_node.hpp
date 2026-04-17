#pragma once

#include "expression_node.hpp"

/// @class `VariantUnwrapNode`
/// @brief Represents variant unwrapping, which is always forced
class VariantUnwrapNode : public ExpressionNode {
  public:
    VariantUnwrapNode(std::unique_ptr<ExpressionNode> &base_expr, const std::shared_ptr<Type> &unwrap_type, const uint8_t unwrap_id) :
        ExpressionNode(base_expr->is_const),
        base_expr(std::move(base_expr)),
        unwrap_id(unwrap_id) {
        this->type = unwrap_type;
    }

    Variation get_variation() const override {
        return Variation::VARIANT_UNWRAP;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::unique_ptr<ExpressionNode> base_expr_clone = base_expr->clone(scope_id);
        return std::make_unique<VariantUnwrapNode>(base_expr_clone, type, unwrap_id);
    }

    /// @var `base_expr`
    /// @brief The base expression to unwrap
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `unwrap_id`
    /// @brief The id of the type unwrapped of the variant
    uint8_t unwrap_id;
};
