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
        FunctionNode(bool is_aligned,
            bool is_const,
            std::string &name,
            std::vector<std::pair<std::string, std::string>> &parameters,
            std::vector<std::string> &return_types,
            std::vector<StatementNode> &body)
            : is_aligned(is_aligned),
            is_const(is_const),
            name(name),
            parameters(std::move(parameters)), return_types(std::move(return_types)),
            body(std::move(body)) {}

    private:
        /// is_aligned
        ///     Determines whether the function needs to be aligned
        bool is_aligned;
        /// is_const
        ///     Determines whether the function is const, e.g. it cannot access data outise of its arguments
        bool is_const;
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
