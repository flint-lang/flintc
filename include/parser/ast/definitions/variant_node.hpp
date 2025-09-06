#pragma once

#include "parser/ast/ast_node.hpp"
#include "parser/type/type.hpp"

#include <string>
#include <utility>
#include <vector>

/// @class `VariantNode`
/// @brief Represents the variant type definition
class VariantNode : public ASTNode {
  public:
    explicit VariantNode(                                                                         //
        const std::string &file_name,                                                             //
        const unsigned int line,                                                                  //
        const unsigned int column,                                                                //
        const unsigned int length,                                                                //
        const std::string &name,                                                                  //
        std::vector<std::pair<std::optional<std::string>, std::shared_ptr<Type>>> &possible_types //
        ) :
        ASTNode(file_name, line, column, length),
        name(name),
        possible_types(std::move(possible_types)) {}

    /// @var `name`
    /// @brief The name of the variant type
    std::string name;

    /// @var `possible_types`
    /// @brief List of all types the varaint can hold and optionally a tag, if that value is tagged in this variant
    std::vector<std::pair<std::optional<std::string>, std::shared_ptr<Type>>> possible_types;
};
