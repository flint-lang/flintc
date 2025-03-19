#pragma once

#include "parser/ast/call_node_base.hpp"
#include "statement_node.hpp"

/// @class `CallExpressionNode`
/// @brief Represents function or method calls
class CallNodeStatement : public CallNodeBase, public StatementNode {
  public:
    using CallNodeBase::CallNodeBase;

    // deconstructor
    ~CallNodeStatement() override = default;
    // copy operations - deleted because of unique_ptr member
    CallNodeStatement(const CallNodeStatement &) = delete;
    CallNodeStatement &operator=(const CallNodeStatement &) = delete;
    // move operations
    CallNodeStatement(CallNodeStatement &&) = default;
    CallNodeStatement &operator=(CallNodeStatement &&) = delete;
};
