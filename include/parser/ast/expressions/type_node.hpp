#pragma once

#include "expression_node.hpp"

/// @class `TypeNode`
/// @brief Represents types as expressions, for example for variant comparisons or extractions
class TypeNode : public ExpressionNode {
  public:
    TypeNode(const std::shared_ptr<Type> &type) {
        this->type = type;
    }
};
