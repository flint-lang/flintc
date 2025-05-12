#pragma once

#include "parser/ast/ast_node.hpp"
#include "parser/type/type.hpp"

#include <optional>
#include <string>
#include <utility>

/// @class `DataNode`
/// @brief Represents data definitions
class DataNode : public ASTNode {
  public:
    DataNode() = default;
    explicit DataNode(                                                                                        //
        const bool is_shared,                                                                                 //
        const bool is_immutable,                                                                              //
        const bool is_aligned,                                                                                //
        const std::string &name,                                                                              //
        const std::vector<std::tuple<std::string, std::shared_ptr<Type>, std::optional<std::string>>> &fields //
        ) :
        is_shared(is_shared),
        is_immutable(is_immutable),
        is_aligned(is_aligned),
        name(name),
        fields(std::move(fields)) {}

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
    /// @brief The fields of the data. This is a list of all field names, and the value is the field's type
    ///
    /// @details
    ///     - The first value is the name of the field
    ///     - The second value is the type of the field
    ///     - The third value is an optional default value
    std::vector<std::tuple<std::string, std::shared_ptr<Type>, std::optional<std::string>>> fields;
};
