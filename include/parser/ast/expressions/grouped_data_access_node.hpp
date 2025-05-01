#pragma once

#include "expression_node.hpp"
#include "parser/type/group_type.hpp"

#include <string>

/// @class `GroupedDataAccessNode`
/// @brief Represents the accessing of multiple data fields at once
class GroupedDataAccessNode : public ExpressionNode {
  public:
    GroupedDataAccessNode(                                    //
        const std::shared_ptr<Type> &data_type,               //
        const std::string &var_name,                          //
        const std::vector<std::string> &field_names,          //
        const std::vector<unsigned int> &field_ids,           //
        const std::vector<std::shared_ptr<Type>> &field_types //
        ) :
        data_type(data_type),
        var_name(var_name),
        field_names(field_names),
        field_ids(field_ids) {
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(field_types);
        if (Type::add_type(group_type)) {
            this->type = group_type;
        } else {
            // The type was already present, so we set the type of the group expression to the already present type to minimize type
            // duplication
            this->type = Type::get_type_from_str(group_type->to_string()).value();
        }
    }

    /// @var `data_type`
    /// @brief The type of the data the accessed variable has
    std::shared_ptr<Type> data_type;

    /// @var `var_name`
    /// @brief The name of the data variable
    std::string var_name;

    /// @var `field_names`
    /// @brief The name of the accessed fields
    std::vector<std::string> field_names;

    /// @var `field_ids`
    /// @brief The indexes of the fields in the data
    std::vector<unsigned int> field_ids;
};
