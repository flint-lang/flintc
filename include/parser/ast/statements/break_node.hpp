#pragma once

#include "statement_node.hpp"

/// class `BreakNode`
/// @brief Represents break statements
class BreakNode : public StatementNode {
  public:
    // constructor
    BreakNode() = default;
    // deconstructor
    ~BreakNode() override = default;
    // copy operations - deleted because of unique_ptr member
    BreakNode(const BreakNode &) = delete;
    BreakNode &operator=(const BreakNode &) = delete;
    // move operations
    BreakNode(BreakNode &&) = default;
    BreakNode &operator=(BreakNode &&) = default;
};
