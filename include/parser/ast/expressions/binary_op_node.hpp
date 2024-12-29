#ifndef __BINARY_OP_NODE_HPP__
#define __BINARY_OP_NODE_HPP__

#include "../../../lexer/token.hpp"
#include "expression_node.hpp"

#include <memory>
#include <utility>

/// BinaryOpNode
///     Represents binary operations.
class BinaryOpNode : public ExpressionNode {
  public:
    BinaryOpNode(Token operator_token, std::unique_ptr<ExpressionNode> &left, std::unique_ptr<ExpressionNode> &right,
        const std::string &type) :
        operator_token(operator_token),
        left(std::move(left)),
        right(std::move(right)) {
        this->type = type;
    }

    // empty constructor
    BinaryOpNode() = default;
    // deconstructor
    ~BinaryOpNode() override = default;
    // copy operations - disabled due to unique_ptr memeber
    BinaryOpNode(const BinaryOpNode &) = delete;
    BinaryOpNode &operator=(const BinaryOpNode &) = delete;
    // move operations
    BinaryOpNode(BinaryOpNode &&) = default;
    BinaryOpNode &operator=(BinaryOpNode &&) = default;

    Token operator_token{};
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
};

#endif
