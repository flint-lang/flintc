#ifndef __ERROR_NODE_HPP__
#define __ERROR_NODE_HPP__

#include "../ast_node.hpp"

#include <string>

/// ErrorNode
///     Represents error definitions
class ErrorNode : public ASTNode {
    public:

    private:
        /// name
        ///     The name of the new error type
        std::string name;
        /// parent_errors
        ///     The errors that the newly created error extends -- does this need to be a vector or is only single-error-parent-set valid?
        std::vector<std::string> parent_errors;
        /// error_types
        ///     The error types the newly created error contains
        std::vector<std::string> error_types;
};

#endif
