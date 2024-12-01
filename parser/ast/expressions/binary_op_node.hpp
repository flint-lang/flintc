#ifndef __BINARY_OP_NODE_HPP__
#define __BINARY_OP_NODE_HPP__

#include "expression_node.hpp"
#include "../../../lexer/token.hpp"

#include <utility>

/// BinaryOpNode
///     Represents binary operations.
class BinaryOpNode : public ExpressionNode {
    public:
        BinaryOpNode(Token operator_token, ExpressionNode &left, ExpressionNode &right) : operator_token(operator_token), left(std::move(left)), right(std::move(right)) {}
    private:
        Token operator_token;
        ExpressionNode left;
        ExpressionNode right;
};


#endif
