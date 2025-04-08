#pragma once

#include "../expressions/expression_node.hpp"
#include "lexer/token.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>

/// @class `AssignmentShorthandNode`
/// @brief Represents assignment shorthand statements
class AssignmentShorthandNode : public StatementNode {
  public:
    AssignmentShorthandNode(const std::string &type, const std::string &name, Token op, std::unique_ptr<ExpressionNode> &expression) :
        type(type),
        name(name),
        op(op),
        expression(std::move(expression)) {}

    // constructor
    AssignmentShorthandNode() = default;
    // deconstructor
    ~AssignmentShorthandNode() override = default;
    // copy operations - deleted because of unique_ptr member
    AssignmentShorthandNode(const AssignmentShorthandNode &) = delete;
    AssignmentShorthandNode &operator=(const AssignmentShorthandNode &) = delete;
    // move operations
    AssignmentShorthandNode(AssignmentShorthandNode &&) = default;
    AssignmentShorthandNode &operator=(AssignmentShorthandNode &&) = default;

    /// @var `type`
    /// @brief The type of the variable
    std::string type;

    /// @var `name`
    /// @brief The name of the variable being assigned to
    std::string name;

    /// @var `op`
    /// @brief The operator of the assignment shorthand
    Token op;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
