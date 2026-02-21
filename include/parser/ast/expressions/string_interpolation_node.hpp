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

    std::unique_ptr<ExpressionNode> clone() const override {
        std::vector<std::variant<std::unique_ptr<ExpressionNode>, std::unique_ptr<LiteralNode>>> string_content_clone;
        string_content_clone.resize(string_content.size());
        for (const auto &content : string_content) {
            if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(content)) {
                std::unique_ptr<ExpressionNode> cloned_value = std::get<std::unique_ptr<ExpressionNode>>(content)->clone();
                string_content_clone.emplace_back(std::move(cloned_value));
            } else if (std::holds_alternative<std::unique_ptr<LiteralNode>>(content)) {
                std::unique_ptr<ExpressionNode> expr_clone = std::get<std::unique_ptr<LiteralNode>>(content)->clone();
                // Make sure that the `expr_clone` does not get it's destructor called at end of scope
                [[maybe_unused]] auto expr_ptr = expr_clone.release();
                std::unique_ptr<LiteralNode> cloned_value{expr_clone->as<LiteralNode>()};
                string_content_clone.emplace_back(std::move(cloned_value));
            }
        }
        return std::make_unique<StringInterpolationNode>(string_content_clone);
    }

    /// @var `string_content`
    /// @brief A vector of expressions or direct string, forming the whole interpolation chain
    std::vector<std::variant<std::unique_ptr<ExpressionNode>, std::unique_ptr<LiteralNode>>> string_content;
};
