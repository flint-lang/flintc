#pragma once

#include "fip.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/type/type.hpp"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

/// @class `Namespace`
/// @brief Represents a namespace containing public and private symbols
class Namespace {
  public:
    explicit Namespace(const std::string &file_path) :
        file_path(file_path),
        namespace_hash(FIP::string_to_hash(file_path)) {}

    explicit Namespace(std::vector<std::unique_ptr<DefinitionNode>> &definitions, const std::string &file_path) :
        file_path(file_path),
        namespace_hash(FIP::string_to_hash(file_path)) {
        public_symbols.definitions = std::move(definitions);
    }

    /// @struct `PublicSymbols`
    /// @brief All symbols that are publicly accessible from this namespace
    struct PublicSymbols {
        /// @var `types`
        /// @brief All types defined in this namespace
        std::unordered_map<std::string, std::shared_ptr<Type>> types;

        /// @var `definitions`
        /// @brief All top-level definitions (functions, data, entities, etc.) of this namespace
        std::vector<std::unique_ptr<DefinitionNode>> definitions;

        /// @var `aliased_imports`
        /// @brief Aliased imports (these are namespaces themselves)
        std::unordered_map<std::string, std::shared_ptr<Namespace>> aliased_imports;
    };

    /// @struct `PrivateSymbols`
    /// @brief Symbols imported without aliasing (only accessible within this file)
    struct PrivateSymbols {
        /// @var `types`
        /// @brief All types defined in other namespaces which have been imported into this namespace
        std::unordered_map<std::string, std::shared_ptr<Type>> types;

        /// @var `functions`
        /// @brief All functions defined in other namespaces which have been imported into this namespace
        std::unordered_map<std::string, std::vector<FunctionNode *>> functions;
    };

    /// @var `public_symbols`
    /// @brief The public section containing all types, functions and imports of this file
    PublicSymbols public_symbols{};

    /// @var `private_symbols`
    /// @brief The private section containing all types and functions of other files
    PrivateSymbols private_symbols{};

    /// @var `file_path`
    /// @brief The file path this namespace represents
    std::string file_path;

    /// @var `namespace_hash`
    /// @brief Character hash (similar to the FIP file hash) of the file path + file name (for code generation symbol prefixes)
    /// @note This hash is also used to uniquely disambiguate between two namespaces
    std::array<char, 8> namespace_hash;
};
