#pragma once

#include "expression_node.hpp"
#include "parser/ap_float.hpp"
#include "parser/ap_int.hpp"

#include <string>
#include <variant>

/// @struct `LitEnum`
/// @brief The structure representing enum literals ('Type.VALUE' or 'Type.(VALUE, VALUE, ...)')
struct LitEnum {
    std::shared_ptr<Type> enum_type;
    std::vector<std::string> values;
};

/// @struct `LitError`
/// @brief The structure representing error literals (`ErrorType.VALUE` or `ErrorType.VALUE("Message")`)
struct LitError {
    std::shared_ptr<Type> error_type;
    std::string value;
    std::optional<std::unique_ptr<ExpressionNode>> message;
};

/// @struct `LitVariantTag`
/// @brief The structure representing variant tag literals (`VariantType.Tag`)
struct LitVariantTag {
    std::shared_ptr<Type> variant_type;
    std::shared_ptr<Type> variation_type;
};

/// @struct `LitOptional`
/// @brief The structure representing optional literals ('none')
struct LitOptional {};

/// @struct `LitPtr`
/// @brief The structure representing pointer literals (`null`)
struct LitPtr {};

/// @struct `LitInt`
/// @brief The structure representing integer literals
struct LitInt {
    APInt value;
};

/// @struct `LitFloat`
/// @brief The structure representing floating point literals
struct LitFloat {
    APFloat value;
};

/// @struct `LitU8`
/// @brief The structure representing u8 literals
struct LitU8 {
    char value;
};

/// @struct `LitBool`
/// @brief The struct represengint bool literals
struct LitBool {
    bool value;
};

/// @struct `LitStr`
/// @brief The structure representing 'str' literals
struct LitStr {
    std::string value;
};

/// @type `LitValue`
/// @brief The type representing a literal value
using LitValue = std::variant<LitEnum, LitError, LitVariantTag, LitOptional, LitPtr, LitInt, LitFloat, LitU8, LitBool, LitStr>;

/// @class `LiteralNode`
/// @brief Represents literal values
class LiteralNode : public ExpressionNode {
  public:
    explicit LiteralNode(                  //
        LitValue &value,                   //
        const std::shared_ptr<Type> &type, //
        const bool is_folded = false       //
        ) :
        value(std::move(value)),
        is_folded(is_folded) {
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::LITERAL;
    }

    std::unique_ptr<ExpressionNode> clone() const override {
        LitValue value_clone;
        if (std::holds_alternative<LitEnum>(value)) {
            const auto &lit = std::get<LitEnum>(value);
            value_clone = lit;
        } else if (std::holds_alternative<LitError>(value)) {
            const auto &lit = std::get<LitError>(value);
            std::optional<std::unique_ptr<ExpressionNode>> message = std::nullopt;
            if (lit.message.has_value()) {
                message = lit.message.value()->clone();
            }
            value_clone = LitError{
                .error_type = lit.error_type,
                .value = lit.value,
                .message = std::move(message),
            };
        } else if (std::holds_alternative<LitVariantTag>(value)) {
            const auto &lit = std::get<LitVariantTag>(value);
            value_clone = lit;
        } else if (std::holds_alternative<LitOptional>(value)) {
            const auto &lit = std::get<LitOptional>(value);
            value_clone = lit;
        } else if (std::holds_alternative<LitPtr>(value)) {
            const auto &lit = std::get<LitPtr>(value);
            value_clone = lit;
        } else if (std::holds_alternative<LitInt>(value)) {
            const auto &lit = std::get<LitInt>(value);
            value_clone = lit;
        } else if (std::holds_alternative<LitFloat>(value)) {
            const auto &lit = std::get<LitFloat>(value);
            value_clone = lit;
        } else if (std::holds_alternative<LitU8>(value)) {
            const auto &lit = std::get<LitU8>(value);
            value_clone = lit;
        } else if (std::holds_alternative<LitBool>(value)) {
            const auto &lit = std::get<LitBool>(value);
            value_clone = lit;
        } else if (std::holds_alternative<LitStr>(value)) {
            const auto &lit = std::get<LitStr>(value);
            value_clone = lit;
        }
        return std::make_unique<LiteralNode>(value_clone, this->type, is_folded);
    }

    /// @var `value`
    /// @brief The literal value
    LitValue value;

    /// @var `is_folded`
    /// @brief Whether this literal is the result of a fold
    bool is_folded;
};
