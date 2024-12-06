#ifndef __IF_NODE_HPP__
#define __IF_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <vector>
#include <utility>

/// IfNode
///     Represents if statements
class IfNode : public StatementNode {
    public:
        IfNode(ExpressionNode &condition, std::vector<StatementNode> &then_branch, std::vector<StatementNode> &else_branch)
        : condition(condition), then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}
    private:
        /// condition
        ///     The COndition expression
        ExpressionNode condition;
        /// then_branch
        ///     The statements to execute when the condition evaluates to 'true'
        std::vector<StatementNode> then_branch;
        /// else_branch
        ///     The statements to execute when the condition evaluates to 'false'
        std::vector<StatementNode> else_branch;
};

#endif
