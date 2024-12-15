#ifndef __IF_NODE_HPP__
#define __IF_NODE_HPP__

#include "../../../types.hpp"
#include "../expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// IfNode
///     Represents if statements
class IfNode : public StatementNode {
  public:
    IfNode(std::unique_ptr<ExpressionNode> &condition, body_statements &then_branch, body_statements &else_branch)
        : condition(std::move(condition)),
          then_branch(std::move(then_branch)),
          else_branch(std::move(else_branch)) {}

    // constructor
    IfNode() = default;
    // deconstructor
    ~IfNode() override = default;
    // copy operations - deleted because of unique_ptr member
    IfNode(const IfNode &) = delete;
    IfNode &operator=(const IfNode &) = delete;
    // move operations
    IfNode(IfNode &&) = default;
    IfNode &operator=(IfNode &&) = default;

    /// condition
    ///     The COndition expression
    std::unique_ptr<ExpressionNode> condition;
    /// then_branch
    ///     The statements to execute when the condition evaluates to 'true'
    body_statements then_branch;
    /// else_branch
    ///     The statements to execute when the condition evaluates to 'false'
    body_statements else_branch;
};

#endif
