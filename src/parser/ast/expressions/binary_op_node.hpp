#ifndef __BINARY_OP_NODE_HPP__
#define __BINARY_OP_NODE_HPP__

#include "expression_node.hpp"
#include "../../../lexer/token.hpp"

#include <memory>
#include <utility>

/// BinaryOpNode
///     Represents binary operations.
class BinaryOpNode : public ExpressionNode {
    public:
        BinaryOpNode(
            Token operator_token,
            std::unique_ptr<ExpressionNode> &left,
            std::unique_ptr<ExpressionNode> &right)
            : operator_token(operator_token),
            left(std::move(left)),
            right(std::move(right)) {}

        // empty constructor
        BinaryOpNode() = default;
        // deconstructor
        ~BinaryOpNode() override = default;
        // copy operations - disabled due to unique_ptr memeber
        BinaryOpNode(const BinaryOpNode &) = delete;
        BinaryOpNode& operator=(const BinaryOpNode&) = delete;
        // move operations
        BinaryOpNode(BinaryOpNode &&) = default;
        BinaryOpNode& operator=(BinaryOpNode &&) = default;
    private:
        Token operator_token;
        std::unique_ptr<ExpressionNode> left;
        std::unique_ptr<ExpressionNode> right;
};


#endif
