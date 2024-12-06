#ifndef __RETURN_NODE_HPP__
#define __RETURN_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

/// ReturnNode
///     Represents return statements
class ReturnNode : public StatementNode {
    public:
        explicit ReturnNode(ExpressionNode &return_value) : return_value(return_value) {}
    private:
        /// return_value
        ///     The return values expression
        ExpressionNode return_value;
};

#endif
