#ifndef __ERROR_NODE_HPP__
#define __ERROR_NODE_HPP__

#include "../ast_node.hpp"

#include <string>
#include <vector>
#include <utility>

/// ErrorNode
///     Represents error definitions
class ErrorNode : public ASTNode {
    public:
        ErrorNode(std::string &name,
            std::string &parent_error,
            std::vector<std::string> &error_types)
        : name(name),
        parent_error(parent_error),
        error_types(std::move(error_types)) {}
    private:
        /// name
        ///     The name of the new error type
        std::string name;
        /// parent_errors
        ///     The errors that the newly created error extends -- does this need to be a vector or is only single-error-parent-set valid?
        std::string parent_error;
        /// error_types
        ///     The error types the newly created error contains
        std::vector<std::string> error_types;
};

#endif
