#ifndef __RESOLVER_HPP__
#define __RESOLVER_HPP__

#include "parser/ast/definitions/import_node.hpp"
#include "parser/ast/file_node.hpp"

#include <llvm/IR/Module.h>

#include <map>
#include <string>
#include <vector>

/// A dependency can either be a:
/// - vector<string>: The library path (f.e. flint.utils.math)
/// - pair<string, string>:
/// - - Left: The directory of the file relative to the file it imports
/// - - Right: The name of the imported file
using dependency = std::variant<std::vector<std::string>, std::pair<std::string, std::string>>;

class Resolver {
  public:
    static std::map<std::string, std::vector<dependency>> &get_dependency_map() {
        static std::map<std::string, std::vector<dependency>> dep;
        return dep;
    }
    static std::map<std::string, FileNode> &get_file_map() {
        static std::map<std::string, FileNode> files;
        return files;
    }
    static std::map<std::string, llvm::Module> &get_module_map() {
        static std::map<std::string, llvm::Module> modules;
        return modules;
    }

    static void add_dependencies_and_file(const FileNode &file_node);
    static void add_ir(const std::string &file, llvm::Module &module);

  private:
    static std::pair<std::string, std::string> split_string(const std::string &path);
    static dependency create_dependency(const ImportNode &node);
};

#endif
