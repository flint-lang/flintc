#pragma once

#include "parser/ast/ast_node.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

/// @class `ImportNode`
/// @brief Represents the use definitions
class ImportNode : public ASTNode {
  public:
    explicit ImportNode(                                                                                        //
        const std::variant<std::pair<std::optional<std::string>, std::string>, std::vector<std::string>> &path, //
        const std::optional<std::string> &alias                                                                 //
        ) :
        path(path),
        alias(alias) {}

    /// @var `path`
    /// @brief A path is either
    ///   - The relative path to the file (string) + the file name (string) or
    ///   - A sequence of identifiers (for libraries: 'xxx.xxx.xxx') or 'Core.xxx'
    std::variant<std::pair<std::optional<std::string>, std::string>, std::vector<std::string>> path;

    /// @var `alias`
    /// @brief The alias of the import, nullopt if no alias is provided
    std::optional<std::string> alias;
};
