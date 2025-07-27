#pragma once

#include "parser/ast/ast_node.hpp"

#include <string>
#include <vector>

/// @class `ErrorNode`
/// @brief Represents error set definitions
class ErrorNode : public ASTNode {
  public:
    explicit ErrorNode(const std::string &name, const std::string &parent_error, const std::vector<std::string> &values) :
        name(name),
        parent_error(parent_error),
        values(values) {}

    /// @var `name`
    /// @brief The name of the new error type
    std::string name;

    /// @var `parent_error`
    /// @brief The name of the parent error set this error set extends
    std::string parent_error;

    /// @var `values`
    /// @brief The possible error values error set contains
    std::vector<std::string> values;
};
