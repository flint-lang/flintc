#pragma once

#include "expressions/expression_node.hpp"
#include "lexer/token.hpp"

#include <memory>
#include <utility>

/// @class `UnaryOpBase`
/// @brief Represents unary operations
class UnaryOpBase {
  public:
    UnaryOpBase(Token operator_token, std::unique_ptr<ExpressionNode> &operand, const bool is_left) :
        operator_token(operator_token),
        operand(std::move(operand)),
        is_left(is_left) {}

    // deconstructor
    ~UnaryOpBase() = default;
    // copy operations - disabled due to unique_ptr member
    UnaryOpBase(const UnaryOpBase &) = delete;
    UnaryOpBase &operator=(const UnaryOpBase &) = delete;
    // move operations
    UnaryOpBase(UnaryOpBase &&) = default;
    UnaryOpBase &operator=(UnaryOpBase &&) = default;

    /// @var `operator_token`
    /// @brief The token of the unary operation
    Token operator_token{};

    /// @var `operand`
    /// @brief The operand the unary operation is applied to
    std::unique_ptr<ExpressionNode> operand;

    /// @var `is_left`
    /// @brief Whether the operator is on the left of the operand
    bool is_left;
};
