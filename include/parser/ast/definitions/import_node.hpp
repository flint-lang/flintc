#pragma once

#include "parser/ast/definitions/definition_node.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

/// @class `ImportNode`
/// @brief Represents the use definitions
class ImportNode : public DefinitionNode {
  public:
    explicit ImportNode(                                                                                        //
        const std::string &file_name,                                                                           //
        const unsigned int line,                                                                                //
        const unsigned int column,                                                                              //
        const unsigned int length,                                                                              //
        const std::variant<std::pair<std::optional<std::string>, std::string>, std::vector<std::string>> &path, //
        const std::optional<std::string> &alias                                                                 //
        ) :
        DefinitionNode(file_name, line, column, length),
        path(path),
        alias(alias) {}

    Variation get_variation() const override {
        return Variation::IMPORT;
    }

    /// @var `path`
    /// @brief A path is either
    ///   - The relative path to the file (string) + the file name (string) or
    ///   - A sequence of identifiers (for libraries: 'xxx.xxx.xxx') or 'Core.xxx'
    std::variant<std::pair<std::optional<std::string>, std::string>, std::vector<std::string>> path;

    /// @var `alias`
    /// @brief The alias of the import, nullopt if no alias is provided
    std::optional<std::string> alias;
};
