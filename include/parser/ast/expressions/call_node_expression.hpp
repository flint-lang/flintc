#ifndef __CALL_EXPRESSION_NODE_HPP__
#define __CALL_EXPRESSION_NODE_HPP__

#include "expression_node.hpp"
#include "parser/ast/call_node_base.hpp"

#include <memory>
#include <string>
#include <vector>

/// CallExpressionNode
///     Represents function or method calls
class CallNodeExpression : public CallNodeBase, public ExpressionNode {
  public:
    using CallNodeBase::CallNodeBase;

    explicit CallNodeExpression(std::string &function_name, std::vector<std::unique_ptr<ExpressionNode>> &arguments,
        const std::string &type) :
        CallNodeBase(function_name, std::move(arguments), type) {
        ExpressionNode::type = type;
    }

    // deconstructor
    ~CallNodeExpression() override = default;
    // copy operations - deleted because of unique_ptr member
    CallNodeExpression(const CallNodeExpression &) = delete;
    CallNodeExpression &operator=(const CallNodeExpression &) = delete;
    // move operations
    CallNodeExpression(CallNodeExpression &&) = default;
    CallNodeExpression &operator=(CallNodeExpression &&) = delete;
};

#endif
