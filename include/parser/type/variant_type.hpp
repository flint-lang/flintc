#pragma once

#include "parser/ast/definitions/variant_node.hpp"
#include "type.hpp"

#include <algorithm>
#include <variant>

/// @class `VariantType`
/// @brief Represents variant types
class VariantType : public Type {
  public:
    VariantType(const std::variant<VariantNode *const, std::vector<std::shared_ptr<Type>>> &var_or_list, const bool is_err_variant) :
        is_err_variant(is_err_variant),
        var_or_list(var_or_list) {}

    std::string to_string() const override {
        if (std::holds_alternative<std::vector<std::shared_ptr<Type>>>(var_or_list)) {
            auto &types = std::get<std::vector<std::shared_ptr<Type>>>(var_or_list);
            std::string result = "variant<";
            for (auto it = types.begin(); it != types.end(); ++it) {
                if (it != types.begin()) {
                    result += ", ";
                }
                result += (*it)->to_string();
            }
            result += ">";
            return result;
        } else {
            auto &variant_node = std::get<VariantNode *const>(var_or_list);
            return variant_node->name;
        }
    }

    /// @function `get_idx_of_type`
    /// @brief Returns the index of the given type in the type list
    ///
    /// @brief `type` The type to seach for
    /// @return `std::optional<unsigned int>` The index of the given type, nullopt if the given type is not a valid type of this variant
    std::optional<unsigned int> get_idx_of_type(const std::shared_ptr<Type> &type) const {
        if (std::holds_alternative<VariantNode *const>(var_or_list)) {
            const auto &possible_types = std::get<VariantNode *const>(var_or_list)->possible_types;
            for (auto var_it = possible_types.begin(); var_it != possible_types.end(); ++var_it) {
                if (var_it->second == type) {
                    return 1 + std::distance(possible_types.begin(), var_it);
                }
            }
        } else {
            const auto &possible_types = std::get<std::vector<std::shared_ptr<Type>>>(var_or_list);
            const auto &idx = std::find(possible_types.begin(), possible_types.end(), type);
            if (idx != possible_types.end()) {
                return 1 + std::distance(possible_types.begin(), idx);
            }
        }
        return std::nullopt;
    }

    /// @function `get_possible_types`
    /// @brief Returns all the possible types in a uniform manner
    ///
    /// @return `std::vector<std::pair<std::optional<std::string>, std::shared_ptr<Type>>>` The possible types of this variant
    std::vector<std::pair<std::optional<std::string>, std::shared_ptr<Type>>> get_possible_types() const {
        if (std::holds_alternative<VariantNode *const>(var_or_list)) {
            return std::get<VariantNode *const>(var_or_list)->possible_types;
        } else {
            std::vector<std::pair<std::optional<std::string>, std::shared_ptr<Type>>> result;
            const auto &possible_types = std::get<std::vector<std::shared_ptr<Type>>>(var_or_list);
            for (const auto &variation_type : possible_types) {
                result.emplace_back(std::nullopt, variation_type);
            }
            return result;
        }
    }

    /// @var `ìs_err_variant`
    /// @brief Whether this variant type is an error variant. Error variants can only contain error types
    bool is_err_variant;

    /// @var `var_or_list`
    /// @brief The variant node this variant type points to or the types the variant could be directly, for when it was inline-defined
    std::variant<VariantNode *const, std::vector<std::shared_ptr<Type>>> var_or_list;
};
