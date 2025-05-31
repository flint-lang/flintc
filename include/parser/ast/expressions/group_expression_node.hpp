#pragma once

#include "error/error.hpp"
#include "expression_node.hpp"
#include "parser/type/group_type.hpp"

#include <memory>
#include <vector>

/// @class `GroupExpressionNode`
/// @brief Represents group expression values
class GroupExpressionNode : public ExpressionNode {
  public:
    explicit GroupExpressionNode(std::vector<std::unique_ptr<ExpressionNode>> &expressions) :
        expressions(std::move(expressions)) {
        std::vector<std::shared_ptr<Type>> types;
        for (auto it = this->expressions.begin(); it != this->expressions.end(); ++it) {
            if (dynamic_cast<const GroupType *>((*it)->type.get())) {
                // Nested groups are not allowed
                THROW_BASIC_ERR(ERR_PARSING);
                return;
            }
            types.emplace_back((*it)->type);
        }
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(types);
        if (Type::add_type(group_type)) {
            this->type = group_type;
        } else {
            // The type was already present, so we set the type of the group expression to the already present type to minimize type
            // duplication
            this->type = Type::get_type_from_str(group_type->to_string()).value();
        }
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
    static inline unsigned int get_next_group_id() {
        static unsigned int group_id = 0;
        static std::mutex group_id_mutex;
        std::lock_guard<std::mutex> lock(group_id_mutex);
        return group_id++;
    }
};
