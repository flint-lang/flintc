#ifndef __VARIABLE_NODE_HPP__
#define __VARIABLE_NODE_HPP__

#include "expression_node.hpp"

#include <string>

/// VaraibleNode
///     Represents variables or identifiers
class VariableNode : public ExpressionNode {
  public:
    explicit VariableNode(std::string &name)
        : name(name) {}

    /// name
    ///     Name of the variable
    std::string name;
};

#endif
