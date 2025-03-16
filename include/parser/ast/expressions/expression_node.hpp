#pragma once

#include "parser/ast/ast_node.hpp"

#include <string>
#include <variant>
#include <vector>

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

    /// @var `type`
    /// @brief The type of the expression
    ///
    /// @details The type of an expression can either be a single type, or a list of types for groups
    std::variant<std::string, std::vector<std::string>> type;
};
