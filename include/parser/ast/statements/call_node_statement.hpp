#pragma once

#include "parser/ast/call_node_base.hpp"
#include "statement_node.hpp"

/// @class `CallNodeStatement`
/// @brief Represents function or method call statements
class CallNodeStatement : public CallNodeBase, public StatementNode {
  public:
    using CallNodeBase::CallNodeBase;

    Variation get_variation() const override {
        return Variation::CALL;
    }

    // deconstructor
    ~CallNodeStatement() override = default;
    // copy operations - deleted because of unique_ptr member
    CallNodeStatement(const CallNodeStatement &) = delete;
    CallNodeStatement &operator=(const CallNodeStatement &) = delete;
    // move operations
    CallNodeStatement(CallNodeStatement &&) = default;
    CallNodeStatement &operator=(CallNodeStatement &&) = delete;
};
