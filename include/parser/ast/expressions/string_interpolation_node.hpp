#pragma once

#include "expression_node.hpp"
#include "literal_node.hpp"
#include "type_cast_node.hpp"

#include <memory>
#include <variant>

/// @class `StringInterpolationNode`
/// @brief Represents string interpolations
class StringInterpolationNode : public ExpressionNode {
  public:
    StringInterpolationNode(std::vector<std::variant<std::unique_ptr<TypeCastNode>, std::unique_ptr<LiteralNode>>> &string_content) :
        string_content(std::move(string_content)) {
        token_list tokens = {{TOK_IDENTIFIER, "str", 0, 0}};
        this->type = Type::get_type({tokens.begin(), tokens.end()}).value();
    }

    /// @var `string_content`
    /// @brief A vector of expressions or direct string, forming the whole interpolation chain
    std::vector<std::variant<std::unique_ptr<TypeCastNode>, std::unique_ptr<LiteralNode>>> string_content;
};
