#include "resolver/resolver.hpp"
#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/definitions/import_node.hpp"
#include "parser/parser.hpp"
#include "profiler.hpp"

#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <variant>

std::unordered_map<std::string, std::shared_ptr<DepNode>> Resolver::dependency_node_map;
std::mutex Resolver::dependency_node_map_mutex;

std::unordered_map<std::string, std::vector<dependency>> Resolver::dependency_map;
std::mutex Resolver::dependency_map_mutex;

std::unordered_map<std::string, FileNode> Resolver::file_map;
std::mutex Resolver::file_map_mutex;

std::unordered_map<std::string, const llvm::Module *> Resolver::module_map;
std::mutex Resolver::module_map_mutex;

std::unordered_map<std::string, std::filesystem::path> Resolver::path_map;
std::mutex Resolver::path_map_mutex;

/// create_dependency_graph
///     Takes a main file and resolves all file imports, causing the AST generation of all used files
///     Moves ownership of the file_node, so it is considered unsafe to access it after ths function call!
std::shared_ptr<DepNode> Resolver::create_dependency_graph(FileNode &file_node, const std::filesystem::path &path) {
    PROFILE_SCOPE("Create dependency graph");
    // Add the files path to the path map
    const std::string file_name = file_node.file_name;
    Resolver::add_path(file_name, path);
    // Add all dependencies of the file and the file itself to the file map and the dependency map
    // Also return a created DepNode, but its dependants are not created yet
    std::optional<DepNode> base_maybe = Resolver::add_dependencies_and_file(file_node, path);
    if (!base_maybe.has_value()) {
        // The main file already exists, this should not happen
        throw_err(ERR_RESOLVING);
        return {};
    }
    std::shared_ptr<DepNode> base = std::make_shared<DepNode>(base_maybe.value());
    dependency_node_map.emplace(file_name, base);

    std::map<std::string, std::vector<dependency>> open_dependencies;
    open_dependencies[file_name] = dependency_map.at(file_name);

    // Resolve dependencies as long as there are open dependencies
    while (!open_dependencies.empty()) {
        std::map<std::string, std::vector<dependency>> next_dependencies;

        // Extract all duplicates from the open_dependencies map and remove those duplicates from the open_dependencies map
        // The open_dependencies map now only contains the unique open dependencies
        // Only duplicates from which the file has not been parsed yet will be extracted
        auto open_duplicates = extract_duplicates(open_dependencies);

        // Parse current level's files in parallel
        std::vector<std::thread> parsing_threads;

        // For all files in the open dependencies map
        for (const auto &[open_dep_name, deps] : open_dependencies) {
            // For all the dependencies of said file
            parsing_threads.emplace_back(
                [&](const std::string &dep_name, const std::vector<dependency> &dependencies) {
                    // Your parsing code here
                    for (const auto &open_dep_dep : dependencies) {
                        if (std::holds_alternative<std::vector<std::string>>(open_dep_dep)) {
                            // Library reference
                            auto lib_dep = std::get<std::vector<std::string>>(open_dep_dep);
                            // TODO: Implement the fetching (and pre-fetching into local cache) of FlintHub Libraries
                            // TODO: Create FlintHub at all first, lol
                            throw_err(ERR_NOT_IMPLEMENTED_YET);
                            continue;
                        }

                        // File path
                        auto file_dep = std::get<std::pair<std::filesystem::path, std::string>>(open_dep_dep);
                        // Check if the file has been parsed already. If it has been, add the DepNode as weak reference to the root dep node
                        if (file_map.find(file_dep.second) != file_map.end()) {
                            std::lock_guard<std::mutex> lock(dependency_node_map_mutex);
                            std::weak_ptr<DepNode> weak(dependency_node_map.at(file_dep.second));
                            dependency_node_map.at(dep_name)->dependencies.emplace_back(weak);
                            continue;
                        }
                        // File is not yet part of the dependency tree, parse it
                        std::filesystem::path dep_file_path = file_dep.first / file_dep.second;
                        FileNode file = Parser(dep_file_path).parse();
                        // Save the file name, as the file itself moves its ownership in the call below
                        std::string parsed_file_name = file.file_name;
                        // Add all dependencies of the file and the file itself to the file map and the dependency map
                        // Also return a created DepNode, but its dependants are not created yet
                        std::optional<DepNode> base_maybe = Resolver::add_dependencies_and_file(file, file_dep.first);
                        if (!base_maybe.has_value()) {
                            // File already exists in the dependency map, so it can be added to the "root" DepNode as a weak ptr
                            std::lock_guard<std::mutex> lock(dependency_node_map_mutex);
                            std::weak_ptr<DepNode> weak(dependency_node_map.at(file_dep.second));
                            dependency_node_map.at(dep_name)->dependencies.emplace_back(weak);
                            continue;
                        }
                        // Add parsed file to the dependency graph
                        add_path(parsed_file_name, file_dep.first);
                        std::shared_ptr<DepNode> shared_dep = std::make_shared<DepNode>(base_maybe.value());
                        {
                            std::lock_guard<std::mutex> lock(dependency_node_map_mutex);
                            shared_dep->root = dependency_node_map.at(dep_name);
                            dependency_node_map.at(dep_name)->dependencies.emplace_back(shared_dep);
                            dependency_node_map.emplace(file_dep.second, shared_dep);
                        } // The mutex will be unlocked automatically when it goes out of scope

                        // Add all dependencies of this parsed file to be parsed next
                        {
                            std::lock_guard<std::mutex> lock(dependency_map_mutex);
                            if (next_dependencies.find(parsed_file_name) != next_dependencies.end()) {
                                std::vector<dependency> next_deps = dependency_map.at(parsed_file_name);
                                next_dependencies.at(parsed_file_name)
                                    .insert(next_dependencies.at(parsed_file_name).end(), next_deps.begin(), next_deps.end());
                            } else {
                                next_dependencies[parsed_file_name] = dependency_map.at(parsed_file_name);
                            }
                        } // The mutex will be unlocked automatically when it goes out of scope
                    }
                },
                open_dep_name, deps);
        }
        // Wait for all threads to finish
        for (auto &thread : parsing_threads) {
            thread.join();
        }
        // Append the duplicate dependencies to the next_dependencies, so that they will run the next iteration
        for (const auto &[name, deps] : open_duplicates) {
            next_dependencies[name].insert(next_dependencies[name].end(), deps.begin(), deps.end());
        }
        open_dependencies = next_dependencies;
    }

    return base;
}

/// get_dependency_graph_tips
///     Finds all tips of the dependency graph
void Resolver::get_dependency_graph_tips(const std::shared_ptr<DepNode> &dep_node, std::vector<std::weak_ptr<DepNode>> &tips) {
    unsigned int weak_dep_count = 0;
    for (const auto &dep : dep_node->dependencies) {
        if (std::holds_alternative<std::shared_ptr<DepNode>>(dep)) {
            const std::shared_ptr<DepNode> *shared_dep = &std::get<std::shared_ptr<DepNode>>(dep);
            if (shared_dep->get()->dependencies.empty()) {
                tips.emplace_back(*shared_dep);
            } else {
                get_dependency_graph_tips(*shared_dep, tips);
            }
        } else {
            weak_dep_count++;
        }
    }
    if (weak_dep_count == dep_node->dependencies.size()) {
        // Only weak deps, so this dep is the tip
        tips.emplace_back(dep_node);
    }
}

/// extract_duplicates
///     Extracts all duplicate dependencies (files to parse) from the given dependency map
///     Removes those dependencies from the given map too
///     Works by
std::map<std::string, std::vector<dependency>> Resolver::extract_duplicates( //
    std::map<std::string, std::vector<dependency>> &dependency_map           //
) {
    std::map<std::string, std::vector<dependency>> unique_dependencies;
    for (auto it = dependency_map.begin(); it != dependency_map.end();) {
        if (unique_dependencies.find(it->first) == unique_dependencies.end() && file_map.find(it->first) != file_map.end()) {
            unique_dependencies.insert(*it);
            ++it;
            dependency_map.erase(std::prev(it));
        } else {
            ++it;
        }
    }
    auto duplicates = dependency_map;
    dependency_map = unique_dependencies;
    return duplicates;
}

/// clear
///     Clears all maps of the resolver. IMPORTANT: This method NEEDS to be called before the LLVMContext responsible for all Modules get
///     freed!
void Resolver::clear() {
    std::lock_guard<std::mutex> lock_dep_map(dependency_map_mutex);
    std::lock_guard<std::mutex> lock_file_map(file_map_mutex);
    std::lock_guard<std::mutex> lock_mod_map(module_map_mutex);
    std::lock_guard<std::mutex> lock_path_map(path_map_mutex);

    dependency_map.clear();
    file_map.clear();
    module_map.clear();
    path_map.clear();
}

/// add_dependencies_and_file
///     Adds the dependencies of a given file node to the dependency_map
///     Adds the FileNode to the file_map
///     Moves ownership of the file_node, so it is considered unsafe to access it after this function call!
std::optional<DepNode> Resolver::add_dependencies_and_file(FileNode &file_node, const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock_dep_map(dependency_map_mutex);
    std::lock_guard<std::mutex> lock_file_map(file_map_mutex);

    std::string file_name = file_node.file_name;
    if (dependency_map.find(file_name) != dependency_map.end() || file_map.find(file_name) != file_map.end()) {
        return std::nullopt;
    }
    std::vector<dependency> dependencies;

    for (const std::unique_ptr<ASTNode> &node : file_node.definitions) {
        if (const auto *import_node = dynamic_cast<const ImportNode *>(node.get())) {
            dependencies.emplace_back(create_dependency(*import_node, path));
        }
    }

    dependency_map.emplace(file_name, dependencies);
    file_map.emplace(file_name, std::move(file_node));
    return DepNode{file_name, {}, {}};
}

/// add_ir
///     Adds the llvm module to the module_map of the Resolver
void Resolver::add_ir(const std::string &file_name, const llvm::Module *module) {
    std::lock_guard<std::mutex> lock(module_map_mutex);
    module_map.emplace(std::string(file_name), module);
}

/// add_path
///     Adds the path to the path_map of the Resolver
void Resolver::add_path(const std::string &file_name, const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(path_map_mutex);
    std::string file_name_copy = file_name;
    std::string path_copy = path;
    path_map.emplace(file_name_copy, path_copy);
}

/// create_dependency
///     Creates a dependency struct from a given ImportNode
dependency Resolver::create_dependency(const ImportNode &node, const std::filesystem::path &path) {
    dependency dep;
    if (std::holds_alternative<std::string>(node.path)) {
        std::string fileName = std::get<std::string>(node.path);
        dep = std::make_pair(path, fileName);
    } else {
        dep = std::get<std::vector<std::string>>(node.path);
    }
    return dep;
}
