#ifndef __WHILE_NODE_HPP__
#define __WHILE_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <memory>

/// WhileNode
///     Represents while loops
class WhileNode : public StatementNode {
    public:

    private:
        /// condition
        ///     The condition expression
        std::shared_ptr<ExpressionNode> condition;
        /// body
        ///     The body of the while loop
        std::shared_ptr<StatementNode> body;
};

#endif
