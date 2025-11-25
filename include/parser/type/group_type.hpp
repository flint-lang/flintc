#pragma once

#include "parser/type/type.hpp"

#include <vector>

/// @class `GroupType`
/// @brief Represents group types
class GroupType : public Type {
  public:
    GroupType(const std::vector<std::shared_ptr<Type>> &types) :
        types(types) {}

    Variation get_variation() const override {
        return Variation::GROUP;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::GROUP) {
            return false;
        }
        const GroupType *const other_type = other->as<GroupType>();
        if (types.size() != other_type->types.size()) {
            return false;
        }
        for (size_t i = 0; i < types.size(); i++) {
            if (!types.at(i)->equals(other_type->types.at(i))) {
                return false;
            }
        }
        return true;
    }

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
