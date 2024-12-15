#ifndef __RETURN_NODE_HPP__
#define __RETURN_NODE_HPP__

#include "../expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// ReturnNode
///     Represents return statements
class ReturnNode : public StatementNode {
  public:
    explicit ReturnNode(std::unique_ptr<ExpressionNode> &return_value)
        : return_value(std::move(return_value)) {}

    // constructor
    ReturnNode() = default;
    // deconstructor
    ~ReturnNode() override = default;
    // copy operations - deleted because of unique_ptr member
    ReturnNode(const ReturnNode &) = delete;
    ReturnNode &operator=(const ReturnNode &) = delete;
    // move operations
    ReturnNode(ReturnNode &&) = default;
    ReturnNode &operator=(ReturnNode &&) = default;

    /// return_value
    ///     The return values expression
    std::unique_ptr<ExpressionNode> return_value;
};

#endif
