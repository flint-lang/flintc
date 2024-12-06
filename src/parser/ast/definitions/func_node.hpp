#ifndef __FUNC_NODE_HPP__
#define __FUNC_NODE_HPP__

#include "../ast_node.hpp"
#include "function_node.hpp"

#include <string>
#include <vector>
#include <utility>

/// FuncNode
///     Represents func module definitions
class FuncNode : public ASTNode {
    public:
        FuncNode() = default;
        explicit FuncNode(std::string &name,
            std::vector<std::pair<std::string, std::string>> &required_data,
            std::vector<FunctionNode> &functions)
            : name(name),
            required_data(std::move(required_data)),
            functions(std::move(functions)) {}
    private:
        /// name
        ///     The name of the func module
        std::string name;
        /// required_data
        ///     The data types that are required by the func and their variable names
        std::vector<std::pair<std::string, std::string>> required_data;
        /// functions
        ///     The functions defined inside the func module. These functions get passed the required data as arguments by default
        std::vector<FunctionNode> functions;
};

#endif
