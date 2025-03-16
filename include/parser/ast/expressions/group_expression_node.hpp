#pragma once

#include "error/error.hpp"
#include "expression_node.hpp"

#include <memory>
#include <string>
#include <variant>
#include <vector>

/// @class `GroupExpressionNode`
/// @brief Represents group expression values
class GroupExpressionNode : public ExpressionNode {
  public:
    explicit GroupExpressionNode(std::vector<std::unique_ptr<ExpressionNode>> &expressions) :
        expressions(std::move(expressions)) {
        std::vector<std::string> types;
        for (auto it = this->expressions.begin(); it != this->expressions.end(); ++it) {
            if (std::holds_alternative<std::vector<std::string>>((*it)->type)) {
                // Nested groups are not allowed
                THROW_BASIC_ERR(ERR_PARSING);
                return;
            }
            types.emplace_back(std::get<std::string>((*it)->type));
        }
        this->type = types;
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
