#pragma once

#include "../ast_node.hpp"

#include <string>

/// OptNode
///     Represents optional types
class OptNode : public ASTNode {
  public:
    explicit OptNode(std::string &type) :
        type(type) {}

  private:
    /// type
    ///     The type that is contained inside the opt
    std::string type;
};
