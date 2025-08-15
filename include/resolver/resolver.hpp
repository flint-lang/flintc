#pragma once

#include "parser/ast/definitions/import_node.hpp"
#include "parser/ast/file_node.hpp"

#include <llvm/IR/Module.h>

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/// A dependency can either be a:
/// - vector<string>: The library path (f.e. flint.utils.math)
/// - pair<std::filesystem::path, string>:
/// - - Left: The directory of the file relative to the file it imports
/// - - Right: The name of the imported file
using dependency = std::variant<std::vector<std::string>, std::pair<std::filesystem::path, std::string>>;

/// @struct `DepNode`
/// @brief Forms an edge of the dependency graph and the dependencies are the lines to other edges
struct DepNode {
    /// @var `file_name`
    /// @brief The name of the dependency file
    std::string file_name;

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

    /// @var `file_map`
    /// @brief A map which contains all FileNodes of the whole program (This map owns the whole AST of all parsed files)
    static inline std::unordered_map<std::string, FileNode *> file_map;

    /// @var `file_ids`
    /// @brief A vector where the index of the element is equal to the file_id
    static inline std::vector<std::string> file_ids;

    /// @var `file_map_mutex`
    /// @brief A mutex for the `file_map` variable, to make accessing the `file_map` thread-safe
    static inline std::mutex file_map_mutex;

    /// @function `create_dependency_graph`
    /// @brief This function parses all files that the `file_node` depends on, causing parsing of the whole dependency tree
    ///
    /// @param `file_node` The "main" file node at which the tree starts, the "root" file of the whole program (the main file)
    /// @param `path` The path the "main" file is located at
    /// @param `run_in_parallel` Whether to parse the files in parallel
    /// @return `std::optional<std::shared_ptr<DepNode>>` A shared pointer to the root of the created dependency graph, nullop if creation
    /// failed
    static std::optional<std::shared_ptr<DepNode>> create_dependency_graph( //
        FileNode *file_node,                                                //
        const std::filesystem::path &path,                                  //
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
    /// @brief Adds the given file name to the list of finished files to "remember" for which files code generation is already done
    ///
    /// @param `file_name` The name of the file for which generation is finished
    static void file_generation_finished(const std::string &file_name);

    /// @function `generated_files_contain`
    /// @brief Checks whether the `generated_files` list contains a given `file_name`
    ///
    /// @param `file_name` The file name to search for in the `generated_files` list
    /// @return `bool` Whether the `generated_files` list contains the given `file_name`
    static bool generated_files_contain(const std::string &file_name);

    /// @function `add_path`
    /// @brief Adds the path to the path_map of the Resolver
    ///
    /// @param `file_name` The name of the file from which the path gets added
    /// @param `path` The path to the given file_name
    static void add_path(const std::string &file_name, const std::filesystem::path &path);

    /// @function `get_path`
    /// @brief Returns the path to the given file
    ///
    /// @param `file_name` The name of the file from which to get the path
    /// @return `std::filesystem::path` The path to the given `file_name`
    static std::filesystem::path get_path(const std::string &file_name);

    /// @function `get_file_from_name`
    /// @brief Returns a reference to the file node from the given `file_name`
    ///
    /// @param `file_name` The name of the file from which to get the referece to the FileNode
    /// @return `FileNode *` A pointer to the FileNode saved within the `file_map`, owned by a `Parser` instance
    static FileNode *get_file_from_name(const std::string &file_name);

  private:
    /// @var `generated_files`
    /// @brief A small list which contains the names of all files whose IR code has already been generated
    static inline std::vector<std::string> generated_files;

    /// @var `generated_files_mutex`
    /// @brief A mutex for the `generated_files` variable, to make accessing it thread-safe
    static inline std::mutex generated_files_mutex;

    /// @var `dependency_node_map`
    /// @brief A map that links all file names to their respective dependency nodes
    static inline std::unordered_map<std::string, std::shared_ptr<DepNode>> dependency_node_map;

    /// @var `dependency_node_map_mutex`
    /// @brief A mutex for the `dependency_node_map` variable, to make accessing it thread-safe
    static inline std::mutex dependency_node_map_mutex;

    /// @var `dependency_map`
    /// @brief A map that links all file names to their respective dependencies
    static inline std::unordered_map<std::string, std::vector<dependency>> dependency_map;

    /// @var `dependency_map_mutex`
    /// @brief A mutex for the `dependency_map` variable, to make accessing it thread-safe
    static inline std::mutex dependency_map_mutex;

    /// @var `path_map`
    /// @brief A map that links all file names to their respective file paths
    static inline std::unordered_map<std::string, std::filesystem::path> path_map;

    /// @var `path_map_mutex`
    /// @brief A mutex for the `path_map` variable, to make accessing it thread-safe
    static inline std::mutex path_map_mutex;

    /// @function `process_dependencies_parallel`
    /// @brief A helper function for the `create_dependency_graph` function to process all the next dependencies in parallel
    ///
    /// @param `open_dependencies` All open dependencies to process
    /// @param `next_dependencies` The map where to write the next dependencies to, which have to be processed next
    /// @return `bool` Whether all dependency processing was successful, false if anything failed
    static bool process_dependencies_parallel(                             //
        std::map<std::string, std::vector<dependency>> &open_dependencies, //
        std::map<std::string, std::vector<dependency>> &next_dependencies  //
    );

    /// @function `process_dependency_file`
    /// @brief A helper function for the `create_dependency_graph` function to process the dependencies of a file single-threaded
    ///
    /// @param `dep_name` The name of the dependency to process
    /// @param `dependencies` The dependencies the given dependency (`dep_name`) has
    /// @param `next_dependencies` The map where to write the next dependencies to, which have to be processed next
    /// @return `bool` Whether the processing of the dependency was successful, false if it failed
    static bool process_dependency_file(                                  //
        const std::string &dep_name,                                      //
        const std::vector<dependency> &dependencies,                      //
        std::map<std::string, std::vector<dependency>> &next_dependencies //
    );

    /// @function `create_dependency`
    /// @brief Creates a dependency struct from a given ImportNode
    ///
    /// @param `node` The import node from which to create the dependency from
    /// @param `path` The path in which the file the ImportNode is from is contained
    /// @return `dependency` The created dependency
    static dependency create_dependency(const ImportNode &node, const std::filesystem::path &path);

    /// @function `add_dependencies_and_file`
    /// @brief Adds the dependencies of a given file node to the dependency_map and adds the FileNode itself to the file_map
    ///
    /// @param `file_node` The file node to add to the maps
    /// @param `path` The directory the given `file_node` is contained in
    /// @return `std::optional<DepNode>` The added dependency node, nullopt if adding the dependency node failed
    static std::optional<DepNode> add_dependencies_and_file(FileNode *file_node, const std::filesystem::path &path);

    /// @function `extract_duplicates`
    /// @brief Extracts all duplicate dependencies (files to parse) from the given dependency map
    ///
    /// @param `dependency_map` The map of dependencies, from which all duplicates have to be removed
    /// @return `std::map<std::string, std::vector<dependency>>` A map containing all the duplicate dependencies which have been removed
    /// from the `dependency_map`
    ///
    /// @attention Modifies the given map by removing the dependencies from it
    static std::map<std::string, std::vector<dependency>> extract_duplicates( //
        std::map<std::string, std::vector<dependency>> &dependency_map        //
    );
};
