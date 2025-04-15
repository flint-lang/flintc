#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/scope.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// @class `WhileNode`
/// @brief Represents while loops
class WhileNode : public StatementNode {
  public:
    WhileNode(std::unique_ptr<ExpressionNode> &condition, std::unique_ptr<Scope> &scope) :
        condition(std::move(condition)),
        scope(std::move(scope)) {}

    // constructor
    WhileNode() = default;
    // deconstructor
    ~WhileNode() override = default;
    // copy operations - deleted because of unique_ptr member
    WhileNode(const WhileNode &) = delete;
    WhileNode &operator=(const WhileNode &) = delete;
    // move operations
    WhileNode(WhileNode &&) = default;
    WhileNode &operator=(WhileNode &&) = default;

    /// @var `condition`
    /// @brief The condition expression
    std::unique_ptr<ExpressionNode> condition;

    /// @var `body`
    /// @brief The body of the while loop
    std::unique_ptr<Scope> scope;
};
