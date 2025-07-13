#pragma once

#include "parser/ast/expressions/expression_node.hpp"

#include <memory>

/// @class `SwitchBase`
/// @brief Represents the base class for common data and utility of switch statements and expressions
class SwitchBase {
  public:
    explicit SwitchBase(std::unique_ptr<ExpressionNode> &switcher) :
        switcher(std::move(switcher)) {}

    /// @var `switcher`
    /// @brief The expression to switch on, e.g. whatever is written after the `switch` keyword
    std::unique_ptr<ExpressionNode> switcher;
};
