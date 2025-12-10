#pragma once

#include "parser/ast/definitions/import_node.hpp"
#include "parser/hash.hpp"
#include "types.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

class DefinitionNode;
class FileNode;

/// @class `Namespace`
/// @brief Represents a namespace containing public and private symbols
class Namespace {
  public:
    explicit Namespace(const std::string &core_module_name) :
        file_path(""),
        namespace_hash(Hash(core_module_name)) {}

    explicit Namespace(const std::filesystem::path &file_path) :
        file_path(std::filesystem::absolute(file_path)),
        namespace_hash(Hash(std::filesystem::absolute(file_path))) {}

    explicit Namespace(std::vector<std::unique_ptr<DefinitionNode>> &definitions, const std::filesystem::path &file_path) :
        file_path(std::filesystem::absolute(file_path)),
        namespace_hash(Hash(std::filesystem::absolute(file_path))) {
        public_symbols.definitions = std::move(definitions);
    }

    /// @struct `PublicSymbols`
    /// @brief All symbols that are publicly accessible from this namespace
    struct PublicSymbols {
        /// @var `types`
        /// @brief All types defined in this namespace
        std::unordered_map<std::string, std::shared_ptr<Type>> types;

        /// @var `unknown_types`
        /// @brief A type register map to track all unknown types of this namespace
        std::unordered_map<std::string, std::shared_ptr<Type>> unknown_types;

        /// @var `definitions`
        /// @brief All top-level definitions (functions, data, entities, etc.) of this namespace
        std::vector<std::unique_ptr<DefinitionNode>> definitions;

        /// @var `imports`
        /// @brief List of all imports of this file, this owns all import nodes. This prevents the import nodes to appear in the public
        /// definitions list
        std::vector<std::unique_ptr<ImportNode>> imports;

        /// @var `aliased_imports`
        /// @brief Aliased imports (these are namespaces themselves)
        std::unordered_map<std::string, Namespace *> aliased_imports;
    };

    /// @struct `PrivateSymbols`
    /// @brief Symbols imported without aliasing (only accessible within this file)
    struct PrivateSymbols {
        /// @var `types`
        /// @brief All types defined in other namespaces which have been imported into this namespace
        std::unordered_map<std::string, std::shared_ptr<Type>> types;

        /// @var `functions`
        /// @brief All functions defined in other namespaces which have been imported into this namespace
        std::unordered_map<Hash, std::vector<FunctionNode *>> functions;
    };

    /// @var `public_symbols`
    /// @brief The public section containing all types, functions and imports of this file
    PublicSymbols public_symbols{};

    /// @var `private_symbols`
    /// @brief The private section containing all types and functions of other files
    PrivateSymbols private_symbols{};

    /// @var `file_path`
    /// @brief The file path this namespace represents
    std::filesystem::path file_path;

    /// @var `namespace_hash`
    /// @brief Character hash (similar to the FIP file hash) of the file path + file name (for code generation symbol prefixes)
    /// @note This hash is also used to uniquely disambiguate between two namespaces
    Hash namespace_hash;

    /// @var `file_node`
    /// @brief A pointer back to the file node this namespace is contained inside. Because file nodes are unique pointers too this pointer
    /// will stay relevant and correct over the program's lifetime
    FileNode *file_node{nullptr};

    /// @function `get_type_from_str`
    /// @brief Finds the type from the given string in this namespace's available types
    ///
    /// @param `type_str` The type string to search for
    /// @return `std::optional<std::shared_ptr<Type>>` The found type, nullopt if the
    std::optional<std::shared_ptr<Type>> get_type_from_str(const std::string &type_str) const;

    /// @function `get_namespace_from_alias`
    /// @brief Returns the namespace imported through an alias
    ///
    /// @param `alias` The alias to get the namespace from
    /// @return `std::optional<Namespace *>` The namespace from the given alias
    std::optional<Namespace *> get_namespace_from_alias(const std::string &alias) const;

    /// @function `get_functions_from_call_types`
    /// @brief Returns all possible functions from the given argument types and the function name found in this namespace
    ///
    /// @param `fn_name` The name of the function which is called
    /// @param `arg_types` The types of the arguments of the call
    /// @param `is_aliased` Whether the this namespace is the current file's namespace (for non-aliased calls the private functions are
    /// available too, but for aliased calls only publically available functions are visible)
    /// @return `std::vector<FunctionNode *>` A simple list of all possible functions which would match the name and arg types
    std::vector<FunctionNode *> get_functions_from_call_types( //
        const std::string &fn_name,                            //
        const std::vector<std::shared_ptr<Type>> &arg_types,   //
        const bool is_aliased                                  //
    ) const;

    /// @function `get_type`
    /// @brief Creates a type from the given list of tokens and adds it to the public or global type list of this namespace
    ///
    /// @param `tokens` The tokens to create a type from
    /// @return `std::optional<std::shared_ptr<Type>>` The type the tokens represent, nullopt if not convertible to any type
    std::optional<std::shared_ptr<Type>> get_type(const token_slice &tokens);

    /// @function `add_type`
    /// @brief Adds the given type to the public or global type list
    ///
    /// @param `type` The type to add to this namespace
    /// @return `bool` Whether the type was newly added (true) or already present (false) in this namespace
    bool add_type(const std::shared_ptr<Type> &type);

    /// @function `resolve_type`
    /// @brief Resolves the given type recursively to search and resolve all unknown types in it
    ///
    /// @param `type` The type to resolve
    /// @return `bool` Whether resolving was successful
    bool resolve_type(std::shared_ptr<Type> &type);

    /// @function `create_type`
    /// @brief Creates a type from a given list of tokens
    ///
    /// @param `tokens` The list of tokens to create the type from
    /// @return `std::optional<Type>` The created type, nullopt if creation failed
    std::optional<std::shared_ptr<Type>> create_type(const token_slice &tokens);

    /// @function `can_be_global`
    /// @brief Checks whether the given type can be put into the global type map, e.g. whether it does not contain any user-defined types
    ///
    /// @param `type` The type to check
    /// @return `bool` Whether the given type is considered to be globally available
    bool can_be_global(const std::shared_ptr<Type> &type);

    /// @function `find_core_function`
    /// @brief Finds the given function from the given core module in this namespace, if available
    ///
    /// @param `module_name` The module from which to get the function
    /// @param `fn_name` The name of the function to search for
    /// @param `arg_types` The types of the arguments with which the function is being called
    /// @return `std::optional<FunctionNode *>` A pointer to the function node if found, nullopt otherwise
    std::optional<FunctionNode *> find_core_function(       //
        const std::string &module_name,                     //
        const std::string &fn_name,                         //
        const std::vector<std::shared_ptr<Type>> &arg_types //
    ) const;
};
