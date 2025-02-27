#ifndef __TYPE_CAST_NODE__
#define __TYPE_CAST_NODE__

#include "expression_node.hpp"

#include <memory>
#include <string>

/// VaraibleNode
///     Represents variables or identifiers
class TypeCastNode : public ExpressionNode {
  public:
    TypeCastNode(const std::string &type, std::unique_ptr<ExpressionNode> expr) :
        expr(std::move(expr)) {
        this->type = type;
    }

    std::unique_ptr<ExpressionNode> expr;
};

#endif
