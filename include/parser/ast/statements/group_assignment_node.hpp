#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @class `GroupAssignmentNode`
/// @brief Represents Group assignment statements
class GroupAssignmentNode : public StatementNode {
  public:
    GroupAssignmentNode(                                                             //
        const std::vector<std::pair<std::shared_ptr<Type>, std::string>> &assignees, //
        std::unique_ptr<ExpressionNode> &expression                                  //
        ) :
        assignees(assignees),
        expression(std::move(expression)) {}

    Variation get_variation() const override {
        return Variation::GROUP_ASSIGNMENT;
    }

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
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> assignees;

    /// @var `expression`
    /// @brief The expression of the assignment
    std::unique_ptr<ExpressionNode> expression;
};
