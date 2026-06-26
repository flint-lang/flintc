#pragma once

#include "statement_node.hpp"

/// class `BreakNode`
/// @brief Represents break statements
class BreakNode : public StatementNode {
  public:
    BreakNode(const Hash &hash, const token_slice &tokens) :
        StatementNode(hash, tokens) {}

    // constructor
    BreakNode() = delete;
    // deconstructor
    ~BreakNode() override = default;
    // copy operations - deleted because of unique_ptr member
    BreakNode(const BreakNode &) = delete;
    BreakNode &operator=(const BreakNode &) = delete;
    // move operations
    BreakNode(BreakNode &&) = default;
    BreakNode &operator=(BreakNode &&) = default;

    Variation get_variation() const override {
        return Variation::BREAK;
    }
};
