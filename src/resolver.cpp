#include "resolver/resolver.hpp"
#include "analyzer/analyzer.hpp"
#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/ast/definitions/import_node.hpp"
#include "parser/parser.hpp"
#include "persistent_thread_pool.hpp"
#include "profiler.hpp"
#include "resolver/resource_lock.hpp"

#include <filesystem>
#include <future>
#include <mutex>
#include <string>
#include <variant>

std::optional<std::shared_ptr<DepNode>> Resolver::create_dependency_graph( //
    FileNode *file_node,                                                   //
    const bool run_in_parallel                                             //
) {
    PROFILE_SCOPE("Create dependency graph");
    // Add the files path to the path map
    const Hash file_hash = file_node->file_namespace->namespace_hash;
    // Add all dependencies of the file and the file itself to the file map and the dependency map
    // Also return a created DepNode, but its dependants are not created yet
    std::optional<DepNode> base_maybe = Resolver::add_dependencies_and_file(file_node);
    if (!base_maybe.has_value()) {
        // The main file already exists, this should not happen
        THROW_BASIC_ERR(ERR_RESOLVING);
        return {};
    }
    std::shared_ptr<DepNode> base = std::make_shared<DepNode>(base_maybe.value());
    dependency_node_map.emplace(file_hash, base);

    std::unordered_map<Hash, std::vector<dependency>> open_dependencies;
    open_dependencies[file_hash] = dependency_map.at(file_hash);
    uint64_t depth = 0;

    // Resolve dependencies as long as there are open dependencies
    while (!open_dependencies.empty() && depth < max_graph_depth) {
        std::unordered_map<Hash, std::vector<dependency>> next_dependencies;

        // Extract all duplicates from the open_dependencies map and remove those duplicates from the open_dependencies map
        // The open_dependencies map now only contains the unique open dependencies
        // Only duplicates from which the file has not been parsed yet will be extracted
        auto open_duplicates = extract_duplicates(open_dependencies);

        bool any_failed = false;
        if (run_in_parallel) {
            any_failed = process_dependencies_parallel(open_dependencies, next_dependencies, depth);
        } else {
            PROFILE_SCOPE("Process Dependencies");
            // Run single-threaded
            for (const auto &[open_dep_hash, deps] : open_dependencies) {
                // For all the dependencies of said file
                if (!process_dependency_file(open_dep_hash, deps, next_dependencies, depth)) {
                    any_failed = true;
                }
            }
        }
        if (any_failed) {
            std::cerr << "Error: Failed to process one or more dependencies" << std::endl;
            return std::nullopt;
        }

        // Append the duplicate dependencies to the next_dependencies, so that they will run the next iteration
        for (const auto &[hash, deps] : open_duplicates) {
            next_dependencies[hash].insert(next_dependencies[hash].end(), deps.begin(), deps.end());
        }
        open_dependencies = next_dependencies;
        depth++;
    }

    return base;
}

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

void Resolver::clear() {
    std::lock_guard<std::mutex> lock_dep_map(dependency_map_mutex);
    std::lock_guard<std::mutex> lock_namespace_map(namespace_map_mutex);
    std::lock_guard<std::mutex> lock_mod_map(generated_files_mutex);

    dependency_map.clear();
    namespace_map.clear();
    file_ids.clear();
    generated_files.clear();
}

void Resolver::file_generation_finished(const Hash &file_hash) {
    std::lock_guard<std::mutex> lock(generated_files_mutex);
    generated_files.emplace_back(file_hash);
}

bool Resolver::generated_files_contain(const Hash &file_hash) {
    return std::find(generated_files.begin(), generated_files.end(), file_hash) != generated_files.end();
}

Namespace *Resolver::get_namespace_from_hash(const Hash &file_hash) {
    std::lock_guard<std::mutex> lock(namespace_map_mutex);
    return namespace_map.at(file_hash);
}

bool Resolver::process_dependencies_parallel(                             //
    std::unordered_map<Hash, std::vector<dependency>> &open_dependencies, //
    std::unordered_map<Hash, std::vector<dependency>> &next_dependencies, //
    const uint64_t depth                                                  //
) {
    PROFILE_THREADED_SCOPE("Process Dependencies", true);
    // Parse current level's files in parallel
    std::vector<std::future<bool>> futures;
    futures.reserve(open_dependencies.size());

    // For all files in the open dependencies map
    for (const auto &[open_dep_name, deps] : open_dependencies) {
        // For all the dependencies of said file
        futures.push_back(thread_pool.enqueue( //
            process_dependency_file,           //
            open_dep_name,                     //
            deps,                              //
            std::ref(next_dependencies),       //
            depth                              //
            ));
    }
    // Check results from all threads
    bool any_failed = false;
    for (auto &future : futures) {
        bool result = future.get();
        if (!result) {
            any_failed = true;
        }
    }
    return any_failed;
}

bool Resolver::process_dependency_file(                                   //
    const Hash &dep_hash,                                                 //
    const std::vector<dependency> &dependencies,                          //
    std::unordered_map<Hash, std::vector<dependency>> &next_dependencies, //
    const uint64_t depth                                                  //
) {
    for (const auto &open_dep_dep : dependencies) {
        if (std::holds_alternative<std::vector<std::string>>(open_dep_dep)) {
            // Library reference
            auto lib_dep = std::get<std::vector<std::string>>(open_dep_dep);
            if (lib_dep.size() == 2 && lib_dep.front() == "Core") {
                // Core imports are not handled here
                continue;
            }
            // TODO: Implement the fetching (and pre-fetching into local cache) of FlintHub Libraries
            // TODO: Create FlintHub at all first, lol
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            continue;
        }

        // File path
        auto file_dep = std::get<FileDependency>(open_dep_dep);
        const Hash file_hash(file_dep.directory / file_dep.filename);
        // Check if the file has been parsed already. If it has been, add the DepNode as weak reference to the root dep node
        ResourceLock file_lock(file_hash.to_string());
        if (namespace_map.find(file_hash) != namespace_map.end()) {
            std::lock_guard<std::mutex> lock(dependency_node_map_mutex);
            std::weak_ptr<DepNode> weak(dependency_node_map.at(file_hash));
            dependency_node_map.at(dep_hash)->dependencies.emplace_back(weak);
            continue;
        }
        // File is not yet part of the dependency tree, parse it
        std::optional<Parser *> parser = Parser::create(file_hash.path);
        // Checking if the path exists is done at the import-clausel creation of the other file
        assert(parser.has_value());
        std::optional<FileNode *> file = parser.value()->parse();
        if (!file.has_value()) {
            std::cerr << "Error: File '" << file_hash.path.filename().string() << "' could not be parsed!" << std::endl;
            return false;
        }
        switch (Analyzer::analyze_file(file.value())) {
            case Analyzer::Result::OK:
                break;
            case Analyzer::Result::ERR_HANDLED:
                std::cerr << "Error: File '" << file_hash.path.filename().string() << "' failed analyze step!" << std::endl;
                return false;
            default:
                std::cerr << "Error: File '" << file_hash.path.filename().string() << "' failed analyze step for unknown reason!"
                          << std::endl;
                return false;
        }
        // Save the file name, as the file itself moves its ownership in the call below
        std::string parsed_file_name = file.value()->file_name;
        // Add all dependencies of the file and the file itself to the file map and the dependency map
        // Also return a created DepNode, but its dependants are not created yet
        std::optional<DepNode> base_maybe = Resolver::add_dependencies_and_file(file.value());
        if (!base_maybe.has_value()) {
            // File already exists in the dependency map, so it can be added to the "root" DepNode as a weak ptr
            std::lock_guard<std::mutex> lock(dependency_node_map_mutex);
            std::weak_ptr<DepNode> weak(dependency_node_map.at(file_hash));
            dependency_node_map.at(dep_hash)->dependencies.emplace_back(weak);
            continue;
        }
        // Add parsed file to the dependency graph
        std::shared_ptr<DepNode> shared_dep = std::make_shared<DepNode>(base_maybe.value());
        {
            std::lock_guard<std::mutex> lock(dependency_node_map_mutex);
            shared_dep->root = dependency_node_map.at(dep_hash);
            dependency_node_map.at(dep_hash)->dependencies.emplace_back(shared_dep);
            dependency_node_map.emplace(file_hash, shared_dep);
        } // The mutex will be unlocked automatically when it goes out of scope

        // Add all dependencies of this parsed file to be parsed next
        // In minimal_tree mode (LSP): at depth 0, parse all imports; at depth > 0, only parse aliased imports
        {
            std::lock_guard<std::mutex> lock(dependency_map_mutex);
            std::vector<dependency> next_deps = dependency_map.at(file_hash);

            // Filter dependencies based on minimal_tree mode, depth, and aliasing
            if (minimal_tree && depth > 0) {
                // At depth > 0 in minimal mode: only add aliased dependencies
                std::vector<dependency> filtered_deps;
                for (const auto &dep : next_deps) {
                    if (std::holds_alternative<std::vector<std::string>>(dep)) {
                        // Always include Core imports
                        filtered_deps.push_back(dep);
                    } else {
                        const auto &fd = std::get<FileDependency>(dep);
                        if (fd.is_aliased) {
                            // Only include aliased file imports at depth > 0
                            filtered_deps.push_back(dep);
                        }
                    }
                }
                next_deps = filtered_deps;
            }
            // At depth == 0 or when !minimal_tree: add all dependencies (no filtering)
            if (!next_deps.empty()) {
                if (next_dependencies.find(file_hash) != next_dependencies.end()) {
                    next_dependencies.at(file_hash).insert(next_dependencies.at(file_hash).end(), next_deps.begin(), next_deps.end());
                } else {
                    next_dependencies[file_hash] = next_deps;
                }
            }
        } // The mutex will be unlocked automatically when it goes out of scope
    }
    return true;
}

dependency Resolver::create_dependency(const ImportNode &node) {
    if (std::holds_alternative<std::vector<std::string>>(node.path)) {
        // Core library import (e.g., Core.print)
        return std::get<std::vector<std::string>>(node.path);
    } else {
        // File import - track if it's aliased
        const auto &path_hash = std::get<Hash>(node.path);
        bool is_aliased = node.alias.has_value();
        return FileDependency(path_hash.path.parent_path(), path_hash.path.filename().string(), is_aliased);
    }
}

std::optional<DepNode> Resolver::add_dependencies_and_file(FileNode *file_node) {
    std::lock_guard<std::mutex> lock_dep_map(dependency_map_mutex);

    Hash file_hash = file_node->file_namespace->namespace_hash;
    if (dependency_map.find(file_hash) != dependency_map.end()) {
        return std::nullopt;
    }
    std::vector<dependency> dependencies;

    for (const std::unique_ptr<ImportNode> &node : file_node->file_namespace->public_symbols.imports) {
        dependencies.emplace_back(create_dependency(*node.get()));
    }

    dependency_map.emplace(file_hash, dependencies);
    file_ids.emplace_back(file_hash);
    return DepNode{file_node->file_name, file_hash, {}, {}};
}

std::unordered_map<Hash, std::vector<dependency>> Resolver::extract_duplicates( //
    std::unordered_map<Hash, std::vector<dependency>> &duplicate_map            //
) {
    std::unordered_map<Hash, std::vector<dependency>> unique_dependencies;
    for (auto it = duplicate_map.begin(); it != duplicate_map.end();) {
        if (unique_dependencies.find(it->first) == unique_dependencies.end() && namespace_map.find(it->first) != namespace_map.end()) {
            unique_dependencies.insert(*it);
            it = duplicate_map.erase(it);
        } else {
            ++it;
        }
    }
    auto duplicates = duplicate_map;
    duplicate_map = unique_dependencies;
    return duplicates;
}
