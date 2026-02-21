#pragma once

#include "expression_node.hpp"
#include "parser/type/group_type.hpp"
#include "resolver/resolver.hpp"

#include <memory>
#include <vector>

/// @class `GroupExpressionNode`
/// @brief Represents group expression values
class GroupExpressionNode : public ExpressionNode {
  public:
    explicit GroupExpressionNode(const Hash &hash, std::vector<std::unique_ptr<ExpressionNode>> &expressions) :
        ExpressionNode(hash),
        expressions(std::move(expressions)) {
        std::vector<std::shared_ptr<Type>> types;
        for (auto it = this->expressions.begin(); it != this->expressions.end(); ++it) {
            types.emplace_back((*it)->type);
        }
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(types);
        Namespace *file_namespace = Resolver::get_namespace_from_hash(file_hash);
        if (file_namespace->add_type(group_type)) {
            this->type = group_type;
        } else {
            // The type was already present, so we set the type of the group expression to the already present type to minimize type
            // duplication
            this->type = file_namespace->get_type_from_str(group_type->to_string()).value();
        }
    }

    Variation get_variation() const override {
        return Variation::GROUP_EXPRESSION;
    }

    std::unique_ptr<ExpressionNode> clone() const override {
        std::vector<std::unique_ptr<ExpressionNode>> expressions_clone;
        for (auto &expr : expressions) {
            expressions_clone.emplace_back(expr->clone());
        }
        return std::make_unique<GroupExpressionNode>(this->file_hash, expressions_clone);
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
