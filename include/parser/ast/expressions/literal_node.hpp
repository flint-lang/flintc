#ifndef __LITERAL_NODE_HPP__
#define __LITERAL_NODE_HPP__

#include "expression_node.hpp"

#include <string>
#include <utility>
#include <variant>

/// LiteralNode
///     Represents literal values
class LiteralNode : public ExpressionNode {
  public:
    explicit LiteralNode(std::variant<int, double, std::string, bool, char> &value)
        : value(std::move(value)) {}

    /// value
    ///     the literal value
    std::variant<int, double, std::string, bool, char> value;
};

#endif
