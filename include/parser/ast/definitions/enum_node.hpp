#pragma once

#include "parser/ast/ast_node.hpp"

#include <string>
#include <vector>

/// @class `EnumNode`
/// @brief Represents enum type definitions
class EnumNode : public ASTNode {
  public:
    explicit EnumNode(                         //
        const std::string &file_name,          //
        const unsigned int line,               //
        const unsigned int column,             //
        const unsigned int length,             //
        const std::string &name,               //
        const std::vector<std::string> &values //
        ) :
        ASTNode(file_name, line, column, length),
        name(name),
        values(values) {}

    /// @var `name`
    /// @brief The name of the enum
    std::string name;

    /// @var `values`
    /// @brief The string values of the enum. Their index in this vector (or their index in the declaration) directly relates to their enum
    /// id (for now).
    std::vector<std::string> values;
};
