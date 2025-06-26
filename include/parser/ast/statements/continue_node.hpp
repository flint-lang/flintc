#pragma once

#include "statement_node.hpp"

/// class `ContinueNode`
/// @brief Represents break statements
class ContinueNode : public StatementNode {
  public:
    // constructor
    ContinueNode() = default;
    // deconstructor
    ~ContinueNode() override = default;
    // copy operations - deleted because of unique_ptr member
    ContinueNode(const ContinueNode &) = delete;
    ContinueNode &operator=(const ContinueNode &) = delete;
    // move operations
    ContinueNode(ContinueNode &&) = default;
    ContinueNode &operator=(ContinueNode &&) = default;
};
