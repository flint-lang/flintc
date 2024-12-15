#ifndef __ASSIGNMENT_NODE_HPP__
#define __ASSIGNMENT_NODE_HPP__

#include "../expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>

/// AssignmentNode
///     Represents assignment statements
class AssignmentNode : public StatementNode {
  public:
    AssignmentNode(std::string &var_name, std::unique_ptr<ExpressionNode> &value)
        : var_name(std::move(var_name)),
          value(std::move(value)) {}

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

    /// var_name
    ///     The name of the variable being assigned to
    std::string var_name;
    /// value
    ///     The value to assign
    std::unique_ptr<ExpressionNode> value;
};

#endif
