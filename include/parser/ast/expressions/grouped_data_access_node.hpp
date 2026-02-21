#pragma once

#include "expression_node.hpp"
#include "parser/type/group_type.hpp"
#include "resolver/resolver.hpp"

#include <string>

/// @class `GroupedDataAccessNode`
/// @brief Represents the accessing of multiple data fields at once
class GroupedDataAccessNode : public ExpressionNode {
  public:
    GroupedDataAccessNode(                                    //
        const Hash &hash,                                     //
        std::unique_ptr<ExpressionNode> &base_expr,           //
        const std::vector<std::string> &field_names,          //
        const std::vector<unsigned int> &field_ids,           //
        const std::vector<std::shared_ptr<Type>> &field_types //
        ) :
        ExpressionNode(hash),
        base_expr(std::move(base_expr)),
        field_names(field_names),
        field_ids(field_ids) {
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(field_types);
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
        return Variation::GROUPED_DATA_ACCESS;
    }

    std::unique_ptr<ExpressionNode> clone() const override {
        std::unique_ptr<ExpressionNode> base_expr_clone = base_expr->clone();
        const std::vector<std::shared_ptr<Type>> &field_types = this->type->as<GroupType>()->types;
        return std::make_unique<GroupedDataAccessNode>(this->file_hash, base_expr_clone, field_names, field_ids, field_types);
    }

    /// @var `base_expr`
    /// @brief The base expression from which the fields are accessed
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `field_names`
    /// @brief The name of the accessed fields
    std::vector<std::string> field_names;

    /// @var `field_ids`
    /// @brief The indices of the fields in the data
    std::vector<unsigned int> field_ids;
};
