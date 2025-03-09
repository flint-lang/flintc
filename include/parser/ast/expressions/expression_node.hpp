#pragma once

#include "../ast_node.hpp"

#include <string>

/// ExpressionNode
///     Base class for all expressions
class ExpressionNode : public ASTNode {
  protected:
    // constructor
    ExpressionNode() = default;

  public:
    // deconstructor
    ~ExpressionNode() override = default;
    // copy operations - deleted because of unique_ptr member
    ExpressionNode(const ExpressionNode &) = delete;
    ExpressionNode &operator=(const ExpressionNode &) = delete;
    // move operations
    ExpressionNode(ExpressionNode &&) = default;
    ExpressionNode &operator=(ExpressionNode &&) = default;

    /// type
    ///     The type of the expression
    std::string type;
};
