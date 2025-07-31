#pragma once

#include "parser/type/type.hpp"

/// @class `GroupType`
/// @brief Represents group types
class GroupType : public Type {
  public:
    GroupType(const std::vector<std::shared_ptr<Type>> &types) :
        types(types) {}

    std::string to_string() const override {
        std::string type_str = "(";
        for (size_t i = 0; i < types.size(); i++) {
            if (i != 0) {
                type_str += ", ";
            }
            type_str += types[i]->to_string();
        }
        type_str += ")";
        return type_str;
    }

    /// @var `types`
    /// @brief The types of this group
    std::vector<std::shared_ptr<Type>> types;
};
