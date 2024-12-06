#ifndef __RETURN_NODE_HPP__
#define __RETURN_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

/// ReturnNode
///     Represents return statements
class ReturnNode : public StatementNode {
    public:

    private:
        /// return_value
        ///     The return values expression
        ExpressionNode return_value;
};

#endif
