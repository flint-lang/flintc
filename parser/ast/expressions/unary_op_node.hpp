#ifndef __UNARY_OP_NODE_HPP__
#define __UNARY_OP_NODE_HPP__

#include "expression_node.hpp"
#include "../../../lexer/token.hpp"

#include <utility>

/// UnaryOpNode
///     Represents unary operations
class UnaryOpNode : public ExpressionNode {
    public:
        UnaryOpNode(Token operator_token, ExpressionNode &operand): operator_token(operator_token), operand(std::move(operand)) {}
    private:
        Token operator_token;
        ExpressionNode operand;
};


#endif
