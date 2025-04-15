#pragma once

#include "expression_node.hpp"

#include <string>
#include <variant>

/// @class `LiteralNode`
/// @brief Represents literal values
class LiteralNode : public ExpressionNode {
  public:
    explicit LiteralNode(                                               //
        const std::variant<int, float, std::string, bool, char> &value, //
        const std::shared_ptr<Type> &type,                              //
        const bool is_folded = false                                    //
        ) :
        value(value),
        is_folded(is_folded) {
        this->type = type;
    }

    /// @var `value`
    /// @brief The literal value
    std::variant<int, float, std::string, bool, char> value;

    /// @var `is_folded`
    /// @brief Whether this literal is the result of a fold
    bool is_folded;
};
