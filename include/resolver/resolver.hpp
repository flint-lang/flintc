#ifndef __RESOLVER_HPP__
#define __RESOLVER_HPP__

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
/// - pair<string, string>:
/// - - Left: The directory of the file relative to the file it imports
/// - - Right: The name of the imported file
using dependency = std::variant<std::vector<std::string>, std::pair<std::filesystem::path, std::string>>;

/// DepNode
///     Forms an edge of the dependency graph and the dependencies are the lines to other edges
struct DepNode {
    /// file_name
    ///     The name of the dependency file
    std::string file_name;
    /// dependencies
    ///     The dependency nodes this node points towards
    ///     If the dependency is a shared pointer its a direct dependency
    ///     If the dependency is a weak pointer its the tail of a circle
    std::vector<std::variant<std::shared_ptr<DepNode>, std::weak_ptr<DepNode>>> dependencies;
    /// root
    ///     The root of this node
    std::shared_ptr<DepNode> root{nullptr};
};

class Resolver {
  public:
    Resolver() = delete;

    static std::shared_ptr<DepNode> create_dependency_graph(FileNode &file_node, const std::filesystem::path &path);
    static void process_dependency_file(                                  //
        const std::string &dep_name,                                      //
        const std::vector<dependency> &dependencies,                      //
        std::map<std::string, std::vector<dependency>> &next_dependencies //
    );
    static void get_dependency_graph_tips(const std::shared_ptr<DepNode> &dep_node, std::vector<std::weak_ptr<DepNode>> &tips);
    static std::map<std::string, std::vector<dependency>> extract_duplicates( //
        std::map<std::string, std::vector<dependency>> &dependency_map        //
    );

    static std::unordered_map<std::string, std::shared_ptr<DepNode>> dependency_node_map;
    static std::mutex dependency_node_map_mutex;

    static std::unordered_map<std::string, std::vector<dependency>> dependency_map;
    static std::mutex dependency_map_mutex;

    static std::unordered_map<std::string, FileNode> file_map;
    static std::mutex file_map_mutex;

    static std::unordered_map<std::string, const llvm::Module *> module_map;
    static std::mutex module_map_mutex;

    static std::unordered_map<std::string, std::filesystem::path> path_map;
    static std::mutex path_map_mutex;

    static void clear();
    static std::optional<DepNode> add_dependencies_and_file(FileNode &file_node, const std::filesystem::path &path);
    static void add_ir(const std::string &file_name, const llvm::Module *module);
    static void add_path(const std::string &file_name, const std::filesystem::path &path);

  private:
    static dependency create_dependency(const ImportNode &node, const std::filesystem::path &path);
};

#endif
