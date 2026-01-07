#pragma once

#include "parser/ast/instance_call_node_base.hpp"
#include "statement_node.hpp"

/// @class `InstanceCallNodeStatement`
/// @brief Represents instance function or method call statements
class InstanceCallNodeStatement : public InstanceCallNodeBase, public StatementNode {
  public:
    using InstanceCallNodeBase::InstanceCallNodeBase;

    Variation get_variation() const override {
        return Variation::INSTANCE_CALL;
    }

    // deconstructor
    ~InstanceCallNodeStatement() override = default;
    // copy operations - deleted because of unique_ptr member
    InstanceCallNodeStatement(const InstanceCallNodeStatement &) = delete;
    InstanceCallNodeStatement &operator=(const InstanceCallNodeStatement &) = delete;
    // move operations
    InstanceCallNodeStatement(InstanceCallNodeStatement &&) = default;
    InstanceCallNodeStatement &operator=(InstanceCallNodeStatement &&) = delete;
};
