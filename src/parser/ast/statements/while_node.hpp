#ifndef __WHILE_NODE_HPP__
#define __WHILE_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <vector>
#include <utility>

/// WhileNode
///     Represents while loops
class WhileNode : public StatementNode {
    public:
        WhileNode(ExpressionNode &condition, std::vector<StatementNode> &body)
        : condition(condition), body(std::move(body)) {}
    private:
        /// condition
        ///     The condition expression
        ExpressionNode condition;
        /// body
        ///     The body of the while loop
        std::vector<StatementNode> body;
};

#endif
