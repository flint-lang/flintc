#pragma once

#include "expression_node.hpp"
#include "parser/type/optional_type.hpp"

/// @class `OptionalUnwrapNode`
/// @brief Represents optional unwrapping, which is always forced
class OptionalUnwrapNode : public ExpressionNode {
  public:
    OptionalUnwrapNode(std::unique_ptr<ExpressionNode> &base_expr) :
        base_expr(std::move(base_expr)) {
        if (const OptionalType *base_type = dynamic_cast<const OptionalType *>(this->base_expr->type.get())) {
            this->type = base_type->base_type;
        } else {
            this->type = this->base_expr->type;
        }
    }

    /// @var `base_expr`
    /// @brief The base expression from which to access one field's value
    std::unique_ptr<ExpressionNode> base_expr;
};
