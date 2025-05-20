#pragma once

#include "expression_node.hpp"
#include "parser/ast/call_node_base.hpp"

#include <memory>
#include <string>
#include <vector>

/// @class `CallNodeExpression`
/// @brief Represents function or method calls
class CallNodeExpression : public CallNodeBase, public ExpressionNode {
  public:
    explicit CallNodeExpression(                                                  //
        std::string &function_name,                                               //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const bool can_throw,                                                     //
        const std::shared_ptr<Type> &type                                         //
        ) :
        CallNodeBase(function_name, std::move(arguments), can_throw, type) {
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
