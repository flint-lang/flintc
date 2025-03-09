#pragma once

#include "expression_node.hpp"

#include <string>

/// VaraibleNode
///     Represents variables or identifiers
class VariableNode : public ExpressionNode {
  public:
    VariableNode(std::string &name, std::string &type) :
        name(name) {
        this->type = type;
    }

    /// name
    ///     Name of the variable
    std::string name;
};
