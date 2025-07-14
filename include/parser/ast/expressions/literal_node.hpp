#pragma once

#include "expression_node.hpp"

#include <string>
#include <variant>

/// @class `LiteralNode`
/// @brief Represents literal values
class LiteralNode : public ExpressionNode {
  public:
    explicit LiteralNode(                                                                                                                 //
        const std::variant<unsigned long, long, unsigned int, int, double, float, std::string, bool, char, std::optional<void *>> &value, //
        const std::shared_ptr<Type> &type,                                                                                                //
        const bool is_folded = false                                                                                                      //
        ) :
        value(value),
        is_folded(is_folded) {
        this->type = type;
    }

    /// @var `value`
    /// @brief The literal value
    std::variant<unsigned long, long, unsigned int, int, double, float, std::string, bool, char, std::optional<void *>> value;

    /// @var `is_folded`
    /// @brief Whether this literal is the result of a fold
    bool is_folded;
};
