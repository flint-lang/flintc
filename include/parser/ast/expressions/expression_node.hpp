#pragma once

#include "parser/ast/ast_node.hpp"
#include "parser/type/type.hpp"

/// @class `ExpressionNode`
/// @brief Base class for all expressions
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

    /// @var `type`
    /// @brief The type of the expression
    std::shared_ptr<Type> type;
};
