#pragma once

#include "parser/ast/callable_call_node_base.hpp"
#include "statement_node.hpp"

/// @class `CallableCallNodeStatement`
/// @brief Represents call statements of callable variables
class CallableCallNodeStatement : public CallableCallNodeBase, public StatementNode {
  public:
    using CallableCallNodeBase::CallableCallNodeBase;

    Variation get_variation() const override {
        return Variation::CALLABLE_CALL;
    }

    // deconstructor
    ~CallableCallNodeStatement() override = default;
    // copy operations - deleted because of unique_ptr member
    CallableCallNodeStatement(const CallableCallNodeStatement &) = delete;
    CallableCallNodeStatement &operator=(const CallableCallNodeStatement &) = delete;
    // move operations
    CallableCallNodeStatement(CallableCallNodeStatement &&) = default;
    CallableCallNodeStatement &operator=(CallableCallNodeStatement &&) = delete;
};
