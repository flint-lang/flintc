#pragma once

#include "expression_node.hpp"

#include <string>

/// @class `VaraibleNode`
/// @brief Represents variables or identifiers
class VariableNode : public ExpressionNode {
  public:
    VariableNode(const std::string &name, const std::shared_ptr<Type> &type) :
        name(name) {
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::VARIABLE;
    }

    std::unique_ptr<ExpressionNode> clone([[maybe_unused]] const unsigned int scope_id) const override {
        return std::make_unique<VariableNode>(name, type);
    }

    /// @var `name`
    /// @brief Name of the variable
    std::string name;
};
