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
    AssignmentNode(std::string &type, std::string &name, std::unique_ptr<ExpressionNode> &expression) :
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

    /// type
    ///     The type of the variable
    std::string type;
    /// name
    ///     The name of the variable being assigned to
    std::string name;
    /// expression
    ///     The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};

#endif
