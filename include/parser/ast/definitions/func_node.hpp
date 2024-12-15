#ifndef __FUNC_NODE_HPP__
#define __FUNC_NODE_HPP__

#include "../ast_node.hpp"
#include "function_node.hpp"

#include <string>
#include <utility>
#include <vector>

/// FuncNode
///     Represents func module definitions
class FuncNode : public ASTNode {
  public:
    explicit FuncNode(std::string &name, std::vector<std::pair<std::string, std::string>> &required_data,
        std::vector<std::unique_ptr<FunctionNode>> functions)
        : name(name),
          required_data(std::move(required_data)),
          functions(std::move(functions)) {}

    // empty constructor
    FuncNode() = default;
    // destructor
    ~FuncNode() override = default;
    // copy operations - disabled due to unique_ptr member
    FuncNode(const FuncNode &) = delete;
    FuncNode &operator=(const FuncNode &) = delete;
    // move operations
    FuncNode(FuncNode &&) = default;
    FuncNode &operator=(FuncNode &&) = default;

  private:
    /// name
    ///     The name of the func module
    std::string name;
    /// required_data
    ///     The data types that are required by the func and their variable names
    std::vector<std::pair<std::string, std::string>> required_data;
    /// functions
    ///     The functions defined inside the func module. These functions get passed the required data as arguments by
    ///     default
    std::vector<std::unique_ptr<FunctionNode>> functions;
};

#endif
