#include "resolver/resolver.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/definitions/import_node.hpp"

#include <filesystem>
#include <string>
#include <variant>

/// clear
///     Clears all maps of the resolver. IMPORTANT: This method NEEDS to be called before the LLVMContext responsible for all Modules get
///     freed!
void Resolver::clear() {
    get_dependency_map().clear();
    get_file_map().clear();
    get_module_map().clear();
    get_path_map().clear();
}

/// add_dependencies_and_file
///     Adds the dependencies of a given file node to the dependency_map
///     Adds the FileNode to the file_map
///     Moves ownership of the file_node, so it is considered unsafe to access it after this function call!
void Resolver::add_dependencies_and_file(FileNode &file_node, const std::filesystem::path &path) {
    std::string file_name = file_node.file_name;
    if (get_dependency_map().find(file_name) != get_dependency_map().end() || get_file_map().find(file_name) != get_file_map().end()) {
        return;
    }
    std::vector<dependency> dependencies;

    for (const std::unique_ptr<ASTNode> &node : file_node.definitions) {
        if (const auto *import_node = dynamic_cast<const ImportNode *>(node.get())) {
            dependencies.emplace_back(create_dependency(*import_node, path));
        }
    }

    get_dependency_map().emplace(file_name, dependencies);
    get_file_map().emplace(file_name, std::move(file_node));
}

/// add_ir
///     Adds the llvm module to the module_map of the Resolver
void Resolver::add_ir(const std::string &file_name, const llvm::Module *module) {
    get_module_map().emplace(std::string(file_name), module);
}

/// add_path
///     Adds the path to the path_map of the Resolver
void Resolver::add_path(const std::string &file_name, const std::filesystem::path &path) {
    std::string file_name_copy = file_name;
    std::string path_copy = path;
    get_path_map().emplace(file_name_copy, path_copy);
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
