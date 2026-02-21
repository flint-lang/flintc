#pragma once

#include "expression_node.hpp"

/// @class `TypeNode`
/// @brief Represents types as expressions, for example for variant comparisons or extractions
class TypeNode : public ExpressionNode {
  public:
    TypeNode(const std::shared_ptr<Type> &type) {
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::TYPE;
    }

    std::unique_ptr<ExpressionNode> clone() const override {
        return std::make_unique<TypeNode>(this->type);
    }
};
