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

    std::unique_ptr<ExpressionNode> clone([[maybe_unused]] const unsigned int scope_id) const override {
        return std::make_unique<TypeNode>(this->type);
    }
};
