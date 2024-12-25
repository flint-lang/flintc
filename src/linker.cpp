#include "linker/linker.hpp"
#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"

#include <variant>

/// resolve_links
///     Takes a main file and resolves all file imports, causing the AST generation of all used files
///     Moves ownership of the file_node, so it is considered unsafe to access it after ths function call!
void Linker::resolve_links(FileNode &file_node, const std::filesystem::path &path) {
    Resolver::add_path(file_node.file_name, path);
    Resolver::add_dependencies_and_file(file_node, path);

    auto iterator = Resolver::get_file_map().begin();
    while (iterator != Resolver::get_file_map().end()) {
        std::vector<dependency> deps = Resolver::get_dependency_map().at(iterator->first);
        bool file_added = false;
        std::filesystem::path file_path = Resolver::get_path_map().at(iterator->first);
        // Check if the dependency is a library or a file
        for (const auto &dep : deps) {
            if (std::holds_alternative<std::vector<std::string>>(dep)) {
                // Library reference
                auto lib_dep = std::get<std::vector<std::string>>(dep);
                // TODO: Implement the fetching (and pre-fetching into local cache) of FlintHub Libraries
                // TODO: Create FlintHub at all first, lol
                throw_err(ERR_NOT_IMPLEMENTED_YET);
            } else {
                // File path
                auto file_dep = std::get<std::pair<std::filesystem::path, std::string>>(dep);
                if (Resolver::get_file_map().find(file_dep.second) == Resolver::get_file_map().end()) {
                    // File is not yet part of the dependency tree, so lets parse and add it!
                    std::filesystem::path dep_file_path = file_dep.first / file_dep.second;
                    FileNode file = Parser::parse_file(dep_file_path);
                    Resolver::add_path(file.file_name, file_dep.first);
                    Resolver::add_dependencies_and_file(file, file_dep.first);

                    // Restart iteration from the beginning of the map
                    iterator = Resolver::get_file_map().begin();
                    file_added = true;
                }
            }
        }
        if (!file_added) {
            ++iterator;
        }
    }
}
