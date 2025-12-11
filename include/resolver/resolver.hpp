#pragma once

#include "parser/ast/definitions/import_node.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/hash.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/// @struct `FileDependency`
/// @brief Represents a file import with information about whether it's aliased
struct FileDependency {
    /// @var `directory`
    /// @brief Directory of the file relative to the file that imports it
    std::filesystem::path directory;

    /// @var `filename`
    /// @brief Name of the imported file
    std::string filename;

    /// @var `is_aliased`
    /// @brief Whether this import uses an alias (transitive for LSP)
    bool is_aliased;

    FileDependency(std::filesystem::path dir, std::string file, bool aliased) :
        directory(std::move(dir)),
        filename(std::move(file)),
        is_aliased(aliased) {}
};

/// A dependency can either be a:
/// - vector<string>: The library path (f.e. flint.utils.math)
/// - FileDependency: A file import with aliasing information
using dependency = std::variant<std::vector<std::string>, FileDependency>;

/// @struct `DepNode`
/// @brief Forms an edge of the dependency graph and the dependencies are the lines to other edges
struct DepNode {
    /// @var `file_name`
    /// @brief The name of the dependency file
    std::string file_name;

    /// @var `file_hash`
    /// @brief The unique hash based on absolute path of this file
    Hash file_hash;

    /// @var `dependencies`
    /// @brief The dependency nodes this node points towards
    ///     - If the dependency is a shared pointer its a direct dependency
    ///     - If the dependency is a weak pointer its the tail of a circle
    std::vector<std::variant<std::shared_ptr<DepNode>, std::weak_ptr<DepNode>>> dependencies;

    /// @var `root`
    /// @brief The root of this node
    std::shared_ptr<DepNode> root{nullptr};
};

/// @class `Resolver`
/// @brief This class handles everything regarding dependency resolution, dependency graphs and resolving inter-file dependencies
class Resolver {
  public:
    Resolver() = delete;

    /// @var `namespace_map`
    /// @brief A map which contains references to all Namespaces of the whole program. The key is the 8-byte encoded hash of each file's
    /// absolute path
    static inline std::unordered_map<Hash, Namespace *> namespace_map;

    /// @var `file_ids`
    /// @brief A vector where the index of the element is equal to the file_id
    static inline std::vector<Hash> file_ids;

    /// @var `namesapce_map_mutex`
    /// @brief A mutex for the `namespace_map` variable, to make accessing the `namespace_map` thread-safe
    static inline std::mutex namespace_map_mutex;

    /// @var `max_graph_depth`
    /// @brief Number to determine the maximum depth of the graph when resolving the file dependencies
    static inline uint64_t max_graph_depth = UINT64_MAX;

    /// @var `minimal_tree`
    /// @brief When true, only parse aliased imports transitively (for LSP optimization). When false, parse all imports (for compiler)
    static inline bool minimal_tree = false;

    /// @function `create_dependency_graph`
    /// @brief This function parses all files that the `file_node` depends on, causing parsing of the whole dependency tree
    ///
    /// @param `file_node` The "main" file node at which the tree starts, the "root" file of the whole program (the main file)
    /// @param `run_in_parallel` Whether to parse the files in parallel
    /// @return `std::optional<std::shared_ptr<DepNode>>` A shared pointer to the root of the created dependency graph, nullop if creation
    /// failed
    static std::optional<std::shared_ptr<DepNode>> create_dependency_graph( //
        FileNode *file_node,                                                //
        const bool run_in_parallel                                          //
    );

    /// @function `get_dependency_graph_tips`
    /// @brief Finds all tips of the dependency graph and saves them in the `tips` vector
    ///
    /// @param `dep_node` The dependenca node thats the "root" of the search, from which all tips are saved in the `tips` vector
    /// @param `tips` The tips vector where all the tips of the dependency "tree" are saved in
    static void get_dependency_graph_tips(const std::shared_ptr<DepNode> &dep_node, std::vector<std::weak_ptr<DepNode>> &tips);

    /// @function `clear`
    /// @brief Clears all maps and other data of the resolver, to make the resolver clean again
    //
    /// @attention This method needs to be called before the LLVMContext responsible for all Modules get freed, everything else is UB
    static void clear();

    /// @function `file_generation_finished`
    /// @brief Adds the given file hash to the list of finished files to "remember" for which files code generation is already done
    ///
    /// @param `file_hash` The hash of the file for which generation is finished
    static void file_generation_finished(const Hash &file_hash);

    /// @function `generated_files_contain`
    /// @brief Checks whether the `generated_files` list contains a given `file_hash`
    ///
    /// @param `file_hash` The file hash to search for in the `generated_files` list
    /// @return `bool` Whether the `generated_files` list contains the given `file_hash`
    static bool generated_files_contain(const Hash &file_hash);

    /// @function `get_namespace_from_hash`
    /// @brief Returns a reference to the namespace from the given `file_hash`
    ///
    /// @param `file_hash` The hash of the file from which to get the referece to the Namespace
    /// @return `Namespace *` A pointer to the Namespace saved within the `namespace_map`, owned by a `Parser` instance
    static Namespace *get_namespace_from_hash(const Hash &file_hash);

  private:
    /// @var `generated_files`
    /// @brief A small list which contains the hashes of all files whose IR code has already been generated
    static inline std::vector<Hash> generated_files;

    /// @var `generated_files_mutex`
    /// @brief A mutex for the `generated_files` variable, to make accessing it thread-safe
    static inline std::mutex generated_files_mutex;

    /// @var `dependency_node_map`
    /// @brief A map that links all file hashes to their respective dependency nodes
    static inline std::unordered_map<Hash, std::shared_ptr<DepNode>> dependency_node_map;

    /// @var `dependency_node_map_mutex`
    /// @brief A mutex for the `dependency_node_map` variable, to make accessing it thread-safe
    static inline std::mutex dependency_node_map_mutex;

    /// @var `dependency_map`
    /// @brief A map that links all file hashes to their respective dependencies
    static inline std::unordered_map<Hash, std::vector<dependency>> dependency_map;

    /// @var `dependency_map_mutex`
    /// @brief A mutex for the `dependency_map` variable, to make accessing it thread-safe
    static inline std::mutex dependency_map_mutex;

    /// @function `process_dependencies_parallel`
    /// @brief A helper function for the `create_dependency_graph` function to process all the next dependencies in parallel
    ///
    /// @param `open_dependencies` All open dependencies to process
    /// @param `next_dependencies` The map where to write the next dependencies to, which have to be processed next
    /// @param `depth` The current depth in the dependency graph (for minimal_tree filtering)
    /// @return `bool` Whether all dependency processing was successful, false if anything failed
    static bool process_dependencies_parallel(                                //
        std::unordered_map<Hash, std::vector<dependency>> &open_dependencies, //
        std::unordered_map<Hash, std::vector<dependency>> &next_dependencies, //
        const uint64_t depth                                                  //
    );

    /// @function `process_dependency_file`
    /// @brief A helper function for the `create_dependency_graph` function to process the dependencies of a file single-threaded
    ///
    /// @param `dep_hash` The hash of the dependency to process
    /// @param `dependencies` The dependencies the given dependency (`dep_name`) has
    /// @param `next_dependencies` The map where to write the next dependencies to, which have to be processed next
    /// @param `depth` The current depth in the dependency graph (for minimal_tree filtering)
    /// @return `bool` Whether the processing of the dependency was successful, false if it failed
    static bool process_dependency_file(                                      //
        const Hash &dep_hash,                                                 //
        const std::vector<dependency> &dependencies,                          //
        std::unordered_map<Hash, std::vector<dependency>> &next_dependencies, //
        const uint64_t depth                                                  //
    );

    /// @function `create_dependency`
    /// @brief Creates a dependency struct from a given ImportNode
    ///
    /// @param `node` The import node from which to create the dependency from
    /// @return `dependency` The created dependency
    static dependency create_dependency(const ImportNode &node);

    /// @function `add_dependencies_and_file`
    /// @brief Adds the dependencies of a given file node to the dependency_map and adds the FileNode's namespace itself to the
    /// namespace_map
    ///
    /// @param `file_node` The file node to add to the maps
    /// @return `std::optional<DepNode>` The added dependency node, nullopt if adding the dependency node failed
    static std::optional<DepNode> add_dependencies_and_file(FileNode *file_node);

    /// @function `extract_duplicates`
    /// @brief Extracts all duplicate dependencies (files to parse) from the given dependency map
    ///
    /// @param `dependency_map` The map of dependencies, from which all duplicates have to be removed
    /// @return `std::unordered_map<Hash, std::vector<dependency>>` A map containing all the duplicate dependencies which have been removed
    /// from the `dependency_map`
    ///
    /// @attention Modifies the given map by removing the dependencies from it
    static std::unordered_map<Hash, std::vector<dependency>> extract_duplicates( //
        std::unordered_map<Hash, std::vector<dependency>> &dependency_map        //
    );
};
