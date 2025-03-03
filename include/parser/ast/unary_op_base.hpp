#ifndef __UNARY_OP_NODE_HPP__
#define __UNARY_OP_NODE_HPP__

#include "expressions/expression_node.hpp"
#include "lexer/token.hpp"

#include <memory>
#include <utility>

/// UnaryOpBase
///     Represents unary operations
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

    Token operator_token{};
    std::unique_ptr<ExpressionNode> operand;
    bool is_left;
};

#endif
