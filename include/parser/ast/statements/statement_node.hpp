#pragma once

#include "parser/ast/ast_node.hpp"

/// @class `StatementNode`
/// @brief Base class for all statements
class StatementNode : public ASTNode {
  protected:
    // constructor
    StatementNode() = default;

  public:
    // destructor
    ~StatementNode() override = default;
    // copy operations - disabled by default
    StatementNode(const StatementNode &) = delete;
    StatementNode &operator=(const StatementNode &) = delete;
    // move operations
    StatementNode(StatementNode &&) = default;
    StatementNode &operator=(StatementNode &&) = default;
};
