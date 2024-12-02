#ifndef __FUNCTION_NODE_HPP__
#define __FUNCTION_NODE_HPP__

#include "../ast_node.hpp"
#include "../statements/statement_node.hpp"

#include <string>
#include <vector>
#include <utility>

/// FunctionNode
///     Represents function definitions
class FunctionNode : public ASTNode {
    public:
        FunctionNode(std::string &name,
            std::vector<std::pair<std::string, std::string>> &parameters,
            std::vector<std::string> &return_types,
            std::vector<StatementNode> &body)
            : name(name),
            parameters(std::move(parameters)), return_types(std::move(return_types)),
            body(std::move(body)) {}

    private:
        /// name
        ///     The name of the function
        std::string name;
        /// parameters
        ///     Parameter names and types
        std::vector<std::pair<std::string, std::string>> parameters;
        /// return_types
        ///     The types of all return values
        std::vector<std::string> return_types;
        /// body
        ///     The body of the function containing all statements
        std::vector<StatementNode> body;
};

#endif
