#pragma once

#include "parser/ast/ast_node.hpp"
#include "parser/type/type.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/// @class `DataNode`
/// @brief Represents data definitions
class DataNode : public ASTNode {
  public:
    DataNode() = default;
    explicit DataNode(                                                                                               //
        const bool is_shared,                                                                                        //
        const bool is_immutable,                                                                                     //
        const bool is_aligned,                                                                                       //
        const std::string &name,                                                                                     //
        const std::unordered_map<std::string, std::pair<std::shared_ptr<Type>, std::optional<std::string>>> &fields, //
        const std::vector<std::string> &order                                                                        //
        ) :
        is_shared(is_shared),
        is_immutable(is_immutable),
        is_aligned(is_aligned),
        name(name),
        fields(std::move(fields)),
        order(std::move(order)) {}

    /// @function `get_initializer_fields`
    /// @brief Returns the fields of the initializer fields
    ///
    /// @return `std::vector<std::tuple<std::string, std::shared_ptr<Type>, std::optional<std::string>>>` The list of fields where:
    ///         - `std::string` The name of the initializer field
    ///         - `std::shared_ptr<Type>` The type of the initializer field
    ///         - `std::optional<std::string>` The default value of the field
    const std::vector<std::tuple<std::string, std::shared_ptr<Type>, std::optional<std::string>>> get_initializer_fields() {
        std::vector<std::tuple<std::string, std::shared_ptr<Type>, std::optional<std::string>>> initializer_fields;
        for (const std::string &field_name : order) {
            std::pair<std::shared_ptr<Type>, std::optional<std::string>> &field_values = fields.at(field_name);
            initializer_fields.emplace_back(field_name, field_values.first, field_values.second);
        }
        return initializer_fields;
    }

    /// @var `is_shared`
    /// @brief Determines whether the data is shared between multiple entities
    bool is_shared{false};

    /// @var `is_immutable`
    /// @brief Determines whether the data is immutable. Immutable data can only be initialized once
    bool is_immutable{false};

    /// @var `is_aligned`
    /// @brief Determines whether the data is aligned to cache-lines
    bool is_aligned{false};

    /// @var `name`
    /// @brief The name of the data module
    std::string name;

    /// @var `fields`
    /// @brief The fields of the data. This is a map of all field names, and the value is the field's type
    ///
    /// @details
    ///     - The key is the name of the field
    ///     - The value is the type of the field and optionally its default value
    std::unordered_map<std::string, std::pair<std::shared_ptr<Type>, std::optional<std::string>>> fields;

    /// @var `order`
    /// @brief The order of the dada fields in the constructor
    std::vector<std::string> order;
};
