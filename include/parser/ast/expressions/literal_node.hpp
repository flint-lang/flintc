#pragma once

#include "expression_node.hpp"

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

/// @struct `LitU32`
/// @brief The structure representing u32 literals
struct LitU32 {
    unsigned int value;
};

/// @struct `LitU64`
/// @brief The structure representing u64 literals
struct LitU64 {
    unsigned long value;
};

/// @struct `LitI32`
/// @brief The structure representing i32 literals
struct LitI32 {
    int value;
};

/// @struct `LitI64`
/// @brief The structure representing i64 literals
struct LitI64 {
    long value;
};

/// @struct `Litf32`
/// @brief The structure representing f32 literals
struct LitF32 {
    float value;
};

/// @struct `LitF64`
/// @brief The structure representing f64 literals
struct LitF64 {
    double value;
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
using LitValue = std::variant<                                                                                            //
    LitEnum, LitError, LitVariantTag, LitOptional, LitU32, LitU64, LitI32, LitI64, LitF32, LitF64, LitU8, LitBool, LitStr //
    >;

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
