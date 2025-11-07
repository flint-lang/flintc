#pragma once

#include "expression_node.hpp"
#include "literal_node.hpp"

#include <memory>
#include <variant>

/// @class `StringInterpolationNode`
/// @brief Represents string interpolations
class StringInterpolationNode : public ExpressionNode {
  public:
    StringInterpolationNode(std::vector<std::variant<std::unique_ptr<ExpressionNode>, std::unique_ptr<LiteralNode>>> &string_content) :
        string_content(std::move(string_content)) {
        this->type = Type::get_primitive_type("str");
    }

    Variation get_variation() const override {
        return Variation::STRING_INTERPOLATION;
    }

    /// @var `string_content`
    /// @brief A vector of expressions or direct string, forming the whole interpolation chain
    std::vector<std::variant<std::unique_ptr<ExpressionNode>, std::unique_ptr<LiteralNode>>> string_content;
};
