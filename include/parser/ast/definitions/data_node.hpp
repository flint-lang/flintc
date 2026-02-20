#pragma once

#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/type/type.hpp"

#include <string>
#include <utility>

/// @class `DataNode`
/// @brief Represents data definitions
class DataNode : public DefinitionNode {
  public:
    /// @struct `Field`
    /// @brief Struct containing all information about a single field of a data node
    struct Field {
        /// @var `name`
        /// @brief The name of the field
        std::string name;

        /// @var `type`
        /// @brief The type of the field
        std::shared_ptr<Type> type;

        /// @var `initializer`
        /// @brief The initializer, e.g. rhs of the field definition
        std::optional<std::unique_ptr<ExpressionNode>> initializer;
    };

    explicit DataNode(             //
        const Hash &file_hash,     //
        const unsigned int line,   //
        const unsigned int column, //
        const unsigned int length, //
        const bool is_shared,      //
        const bool is_immutable,   //
        const bool is_aligned,     //
        const std::string &name,   //
        std::vector<Field> &fields //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        is_shared(is_shared),
        is_immutable(is_immutable),
        is_aligned(is_aligned),
        name(name),
        fields(std::move(fields)) {}

    Variation get_variation() const override {
        return Variation::DATA;
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
    /// @brief The fields of the data
    std::vector<Field> fields;
};
