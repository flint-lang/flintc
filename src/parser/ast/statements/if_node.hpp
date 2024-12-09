#ifndef __IF_NODE_HPP__
#define __IF_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <vector>
#include <utility>
#include <memory>

/// IfNode
///     Represents if statements
class IfNode : public StatementNode {
    public:
        IfNode(std::unique_ptr<ExpressionNode> &condition,
            std::vector<std::unique_ptr<StatementNode>> &then_branch,
            std::vector<std::unique_ptr<StatementNode>> &else_branch)
        : condition(std::move(condition)),
        then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}

        // constructor
        IfNode() = default;
        // deconstructor
        ~IfNode() override = default;
        // copy operations - deleted because of unique_ptr member
        IfNode(const IfNode &) = delete;
        IfNode& operator=(const IfNode &) = delete;
        // move operations
        IfNode(IfNode &&) = default;
        IfNode& operator=(IfNode &&) = default;
    private:
        /// condition
        ///     The COndition expression
        std::unique_ptr<ExpressionNode> condition;
        /// then_branch
        ///     The statements to execute when the condition evaluates to 'true'
        std::vector<std::unique_ptr<StatementNode>> then_branch;
        /// else_branch
        ///     The statements to execute when the condition evaluates to 'false'
        std::vector<std::unique_ptr<StatementNode>> else_branch;
};

#endif
