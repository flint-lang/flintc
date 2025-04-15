#pragma once

#include "expression_node.hpp"

#include <memory>

/// @class `VaraibleNode`
/// @brief Represents variables or identifiers
class TypeCastNode : public ExpressionNode {
  public:
    TypeCastNode(const std::shared_ptr<Type> &type, std::unique_ptr<ExpressionNode> &expr) :
        expr(std::move(expr)) {
        this->type = type;
    }

    /// @var `expr`
    /// @brief The expression of the type cast node
    std::unique_ptr<ExpressionNode> expr;
};
