#ifndef __IMPORT_NODE_HPP__
#define __IMPORT_NODE_HPP__

#include "../ast_node.hpp"

#include <string>
#include <variant>
#include <vector>

/// ImportNode
///     Represents the use definitions
class ImportNode : public ASTNode {
  public:
    explicit ImportNode(std::variant<std::string, std::vector<std::string>> &path)
        : path(std::move(path)) {}

    /// import_path
    ///     Either the direct file import path (string) or a sequence of namespace declarations (for libraries:
    ///     'xxx.xxx.xxx')
    std::variant<std::string, std::vector<std::string>> path;
};

#endif
