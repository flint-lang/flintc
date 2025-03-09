#pragma once

#include "../ast_node.hpp"

#include <string>
#include <utility>
#include <vector>

/// EnumNode
///     Represents enum type definitions
class EnumNode : public ASTNode {
  public:
    explicit EnumNode(std::string &name, std::vector<std::string> &values) :
        name(name),
        values(std::move(values)) {}

  private:
    /// name
    ///     The name of the enum
    std::string name;
    /// values
    ///     The string values of the enum. Their index in this vector (or their index in the declaration) directly
    ///     relates to their enum id (for now).
    std::vector<std::string> values;
};
