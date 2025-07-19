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
    explicit VariantNode(std::string &name, std::vector<std::shared_ptr<Type>> &possible_types) :
        name(name),
        possible_types(std::move(possible_types)) {}

    /// @var `name`
    /// @brief The name of the variant type
    std::string name;

    /// @var `possible_types`
    /// @brief List of all types the varaint can hold
    std::vector<std::shared_ptr<Type>> possible_types;
};
