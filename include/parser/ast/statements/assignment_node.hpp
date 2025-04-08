#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>

/// @class `AssignmentNode`
/// @brief Represents assignment statements
class AssignmentNode : public StatementNode {
  public:
    AssignmentNode(                                 //
        const std::string &type,                    //
        const std::string &name,                    //
        std::unique_ptr<ExpressionNode> &expression //
        ) :
        type(type),
        name(name),
        expression(std::move(expression)) {}

    // constructor
    AssignmentNode() = default;
    // deconstructor
    ~AssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    AssignmentNode(const AssignmentNode &) = delete;
    AssignmentNode &operator=(const AssignmentNode &) = delete;
    // move operations
    AssignmentNode(AssignmentNode &&) = default;
    AssignmentNode &operator=(AssignmentNode &&) = default;

    /// @var `type`
    /// @brief The type of the variable
    std::string type;

    /// @var `name`
    /// @brief The name of the variable being assigned to
    std::string name;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
