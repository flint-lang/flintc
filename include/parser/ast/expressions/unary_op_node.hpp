#ifndef __UNARY_OP_NODE_HPP__
#define __UNARY_OP_NODE_HPP__

#include "expression_node.hpp"
#include "lexer/token.hpp"

#include <memory>
#include <utility>

/// UnaryOpNode
///     Represents unary operations
class UnaryOpNode : public ExpressionNode {
  public:
    UnaryOpNode(Token operator_token, std::unique_ptr<ExpressionNode> &operand, const bool is_left) :
        operator_token(operator_token),
        operand(std::move(operand)),
        is_left(is_left) {}

    // empty constructor
    UnaryOpNode() = default;
    // deconstructor
    ~UnaryOpNode() override = default;
    // copy operations - disabled due to unique_ptr member
    UnaryOpNode(const UnaryOpNode &) = delete;
    UnaryOpNode &operator=(const UnaryOpNode &) = delete;
    // move operations
    UnaryOpNode(UnaryOpNode &&) = default;
    UnaryOpNode &operator=(UnaryOpNode &&) = default;

    Token operator_token{};
    std::unique_ptr<ExpressionNode> operand;
    bool is_left;
};

#endif
