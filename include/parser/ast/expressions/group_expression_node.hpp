#pragma once

#include "expression_node.hpp"

#include <memory>
#include <string>
#include <vector>

/// GroupExpressionNode
///     Represents group expression values
class GroupExpressionNode : public ExpressionNode {
  public:
    explicit GroupExpressionNode(std::vector<std::unique_ptr<ExpressionNode>> &expressions) :
        expressions(std::move(expressions)) {
        std::string type;
        for (auto it = this->expressions.begin(); it != this->expressions.end(); ++it) {
            if (it == this->expressions.begin()) {
                type += "(" + (*it)->type;
            } else if (it == this->expressions.end()) {
                type += (*it)->type + ")";
            } else {
                type += "," + (*it)->type;
            }
        }
        this->type = type;
    }

    /// @var `expressions`
    /// @brief All the expressions part of the group expression
    std::vector<std::unique_ptr<ExpressionNode>> expressions;

    /// @var `group_id`
    /// @brief The unique id of the group
    const unsigned int group_id = get_next_group_id();

  private:
    /// @function `get_next_group_id`
    /// @brief Returns the next group id. Ensures that each group gets its own id for the lifetime of the program
    static unsigned inline int get_next_group_id() {
        static unsigned int group_id = 0;
        return group_id++;
    }
};
