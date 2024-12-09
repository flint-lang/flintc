#ifndef __UNARY_OP_NODE_HPP__
#define __UNARY_OP_NODE_HPP__

#include "expression_node.hpp"
#include "../../../lexer/token.hpp"

#include <memory>
#include <utility>

/// UnaryOpNode
///     Represents unary operations
class UnaryOpNode : public ExpressionNode {
    public:
        UnaryOpNode(Token operator_token,
            std::unique_ptr<ExpressionNode> &operand)
        : operator_token(operator_token), operand(std::move(operand)) {}

        // empty constructor
        UnaryOpNode() = default;
        // deconstructor
        ~UnaryOpNode() override = default;
        // copy operations - disabled due to unique_ptr member
        UnaryOpNode(const UnaryOpNode &) = delete;
        UnaryOpNode& operator=(const UnaryOpNode &) = delete;
        // move operations
        UnaryOpNode(UnaryOpNode &&) = default;
        UnaryOpNode& operator=(UnaryOpNode &&) = default;
    private:
        Token operator_token{TOK_EOF};
        std::unique_ptr<ExpressionNode> operand;
};


#endif
