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
    explicit ImportNode(                                          //
        const Hash &file_hash,                                    //
        const unsigned int line,                                  //
        const unsigned int column,                                //
        const unsigned int length,                                //
        const std::variant<Hash, std::vector<std::string>> &path, //
        const std::optional<std::string> &alias,                  //
        const bool implicit = false                               //
        ) :
        DefinitionNode(file_hash, line, column, length, {}, {}),
        path(path),
        alias(alias),
        implicit(implicit) {}

    Variation get_variation() const override {
        return Variation::IMPORT;
    }

    /// @var `path`
    /// @brief A path is either
    ///   - The hash of the imported file
    ///   - A sequence of identifiers (for libraries: 'xxx.xxx.xxx') or 'Core.xxx'
    std::variant<Hash, std::vector<std::string>> path;

    /// @var `alias`
    /// @brief The alias of the import, nullopt if no alias is provided
    std::optional<std::string> alias;

    /// @var `implicit`
    /// @brief Whether this import was inserted by the compiler to seed implicit aliases (e.g. dependency names)
    bool implicit;
};
