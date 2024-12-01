#ifndef __ASSIGNMENT_NODE_HPP__
#define __ASSIGNMENT_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <string>
#include <utility>

/// AssignmentNode
///     Represents assignment statements
class AssignmentNode : public StatementNode {
    public:
        AssignmentNode(std::string &var_name, ExpressionNode &value): var_name(std::move(var_name)), value(std::move(value)) {}
    private:
        /// var_name
        ///     The name of the variable being assigned to
        std::string var_name;
        /// value
        ///     The value to assign
        ExpressionNode value;
};

#endif
