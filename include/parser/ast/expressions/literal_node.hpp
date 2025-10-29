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
using LitValue = std::variant<LitEnum, LitError, LitVariantTag, LitOptional, LitInt, LitFloat, LitU8, LitBool, LitStr>;

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

    /// @var `value`
    /// @brief The literal value
    LitValue value;

    /// @var `is_folded`
    /// @brief Whether this literal is the result of a fold
    bool is_folded;
};
