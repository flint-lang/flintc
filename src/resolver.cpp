#include "resolver/resolver.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/definitions/import_node.hpp"

#include <string>
#include <variant>

/// add_dependencies_and_file
///     Adds the dependencies of a given file node to the dependency_map
///     Adds the FileNode to the file_map
///     Moves ownership of the file_node, so it is considered unsafe to access it after this function call!
void Resolver::add_dependencies_and_file(FileNode &file_node) {
    std::string file_name = file_node.file_name;
    if (get_dependency_map().find(file_name) != get_dependency_map().end() || get_file_map().find(file_name) != get_file_map().end()) {
        return;
    }
    std::vector<dependency> dependencies;

    for (const std::unique_ptr<ASTNode> &node : file_node.definitions) {
        if (const auto *import_node = dynamic_cast<const ImportNode *>(node.get())) {
            dependencies.emplace_back(create_dependency(*import_node));
        }
    }

    get_dependency_map().emplace(file_name, dependencies);
    get_file_map().emplace(file_name, std::move(file_node));
}

/// add_ir
///     Adds the llvm module to the module_map of the Resolver
void Resolver::add_ir(const std::string &file_name, std::unique_ptr<const llvm::Module> &module) {
    get_module_map().emplace(std::string(file_name), std::move(module));
}

/// split_string
///     Returns a pair of strings, the first containing the "path" component, the second containing the "basename" (filename) component of
///     the string
std::pair<std::string, std::string> Resolver::split_string(const std::string &path) {
    auto iterator = path.rbegin();
    while (iterator != path.rend() && *iterator != '/') {
        ++iterator;
    }
    std::string split_path(path.begin(), iterator.base());
    std::string base(iterator.base(), path.end());
    return {split_path, base};
}

/// create_dependency
///     Creates a dependency struct from a given ImportNode
dependency Resolver::create_dependency(const ImportNode &node) {
    dependency dep;
    if (std::holds_alternative<std::string>(node.path)) {
        dep = split_string(std::get<std::string>(node.path));
    } else {
        dep = std::get<std::vector<std::string>>(node.path);
    }
    return dep;
}
