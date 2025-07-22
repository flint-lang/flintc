#pragma once

#include "expression_node.hpp"

/// @class `SwitchMatchNode`
/// @brief Represents the match of a switch. This is used primarily for internal reasons
class SwitchMatchNode : public ExpressionNode {
  public:
    SwitchMatchNode(const std::shared_ptr<Type> &type, const std::string &name, const unsigned int id) :
        name(name),
        id(id) {
        this->type = type;
    }

    /// @var `name`
    /// @brief The name of the switch match variable. This name is one of:
    ///           - The variable through which an extracted optional is accessible
    ///           - The variable through which an extracted variant is accessible
    std::string name;

    /// @var `id`
    /// @brief The id of the switch match
    unsigned int id;
};
