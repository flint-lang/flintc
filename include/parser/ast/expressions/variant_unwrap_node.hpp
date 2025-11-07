#pragma once

#include "expression_node.hpp"

/// @class `VariantUnwrapNode`
/// @brief Represents variant unwrapping, which is always forced
class VariantUnwrapNode : public ExpressionNode {
  public:
    VariantUnwrapNode(std::unique_ptr<ExpressionNode> &base_expr, const std::shared_ptr<Type> &unwrap_type) :
        base_expr(std::move(base_expr)) {
        this->type = unwrap_type;
    }

    Variation get_variation() const override {
        return Variation::VARIANT_UNWRAP;
    }

    /// @var `base_expr`
    /// @brief The base expression to unwrap
    std::unique_ptr<ExpressionNode> base_expr;
};
