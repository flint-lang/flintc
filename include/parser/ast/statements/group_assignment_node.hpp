#pragma once

#include "../expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// GroupAssignmentNode
///     Represents Group assignment statements
class GroupAssignmentNode : public StatementNode {
  public:
    GroupAssignmentNode(const std::vector<std::pair<std::string, std::string>> &assignees, std::unique_ptr<ExpressionNode> &expression) :
        assignees(assignees),
        expression(std::move(expression)) {}

    // constructor
    GroupAssignmentNode() = default;
    // deconstructor
    ~GroupAssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    GroupAssignmentNode(const GroupAssignmentNode &) = delete;
    GroupAssignmentNode &operator=(const GroupAssignmentNode &) = delete;
    // move operations
    GroupAssignmentNode(GroupAssignmentNode &&) = default;
    GroupAssignmentNode &operator=(GroupAssignmentNode &&) = default;

    /// @var `assignees`
    /// @brief The types and names of the variables the expression is assigned to
    std::vector<std::pair<std::string, std::string>> assignees;

    /// @var `expression`
    /// @brief The expression of the assignment
    std::unique_ptr<ExpressionNode> expression;
};
