#ifndef __IF_NODE_HPP__
#define __IF_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <memory>

/// IfNode
///     Represents if statements
class IfNode : public StatementNode {
    public:

    private:
        /// condition
        ///     The COndition expression
        std::shared_ptr<ExpressionNode> condition;
        /// then_branch
        ///     The statements to execute when the condition evaluates to 'true'
        std::shared_ptr<StatementNode> then_branch;
        /// else_branch
        ///     The statements to execute when the condition evaluates to 'false'
        std::shared_ptr<StatementNode> else_branch;
};

#endif
