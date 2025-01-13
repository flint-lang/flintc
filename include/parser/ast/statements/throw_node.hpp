#ifndef __THROW_NODE_HPP__
#define __THROW_NODE_HPP__

#include "../expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// ThrowNode
///     Represents throw statements
class ThrowNode : public StatementNode {
  public:
    explicit ThrowNode(std::unique_ptr<ExpressionNode> &throw_value) :
        throw_value(std::move(throw_value)) {}

    // constructor
    ThrowNode() = default;
    // deconstructor
    ~ThrowNode() override = default;
    // copy operations - deleted because of unique_ptr member
    ThrowNode(const ThrowNode &) = delete;
    ThrowNode &operator=(const ThrowNode &) = delete;
    // move operations
    ThrowNode(ThrowNode &&) = default;
    ThrowNode &operator=(ThrowNode &&) = default;

    /// throw_value
    ///     The error value to throw (Numbers for now, something different down the line)
    std::unique_ptr<ExpressionNode> throw_value;
};

#endif
