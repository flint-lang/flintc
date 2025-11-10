#pragma once

#include "expression_node.hpp"
#include "parser/type/optional_type.hpp"

/// @class `OptionalUnwrapNode`
/// @brief Represents optional unwrapping, which is always forced
class OptionalUnwrapNode : public ExpressionNode {
  public:
    OptionalUnwrapNode(std::unique_ptr<ExpressionNode> &base_expr) :
        base_expr(std::move(base_expr)) {
        if (this->base_expr->type->get_variation() == Type::Variation::OPTIONAL) {
            const auto *base_type = this->base_expr->type->as<OptionalType>();
            this->type = base_type->base_type;
        } else {
            this->type = this->base_expr->type;
        }
    }

    Variation get_variation() const override {
        return Variation::OPTIONAL_UNWRAP;
    }

    /// @var `base_expr`
    /// @brief The base expression from which to access one field's value
    std::unique_ptr<ExpressionNode> base_expr;
};
