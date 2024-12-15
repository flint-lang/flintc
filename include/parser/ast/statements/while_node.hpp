#ifndef __WHILE_NODE_HPP__
#define __WHILE_NODE_HPP__

#include "../../../types.hpp"
#include "../expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// WhileNode
///     Represents while loops
class WhileNode : public StatementNode {
  public:
    WhileNode(std::unique_ptr<ExpressionNode> &condition, body_statements &body)
        : condition(std::move(condition)),
          body(std::move(body)) {}

    // constructor
    WhileNode() = default;
    // deconstructor
    ~WhileNode() override = default;
    // copy operations - deleted because of unique_ptr member
    WhileNode(const WhileNode &) = delete;
    WhileNode &operator=(const WhileNode &) = delete;
    // move operations
    WhileNode(WhileNode &&) = default;
    WhileNode &operator=(WhileNode &&) = default;

    /// condition
    ///     The condition expression
    std::unique_ptr<ExpressionNode> condition;
    /// body
    ///     The body of the while loop
    body_statements body;
};

#endif
