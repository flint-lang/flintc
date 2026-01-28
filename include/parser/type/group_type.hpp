#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

#include <algorithm>
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

    bool is_freeable() const override {
        return false;
    }

    Hash get_hash() const override {
        std::vector<Hash> value_hashes;
        for (const auto &type : types) {
            const auto &type_hash = type->get_hash();
            if (type_hash.to_string() == "00000000") {
                continue;
            }
            const auto &equals_fn = [type_hash](const Hash &hash) -> bool { return hash.to_string() == type_hash.to_string(); };
            if (std::find_if(value_hashes.begin(), value_hashes.end(), equals_fn) == value_hashes.end()) {
                value_hashes.emplace_back(type_hash);
            }
        }
        if (value_hashes.empty()) {
            return Hash(std::string(""));
        }
        return value_hashes.front();
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

    std::string get_type_string(const bool is_return_type = false) const override {
        std::string type_str = is_return_type ? "type.ret." : "type.tuple.";
        if (types.empty()) {
            return type_str + "void";
        }
        for (size_t i = 0; i < types.size(); i++) {
            if (i > 0) {
                type_str += "_";
            }
            type_str += types.at(i)->to_string();
        }
        return type_str;
    }

    /// @var `types`
    /// @brief The types of this group
    std::vector<std::shared_ptr<Type>> types;
};
