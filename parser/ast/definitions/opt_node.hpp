#ifndef __OPT_NODE_HPP__
#define __OPT_NODE_HPP__

#include "../ast_node.hpp"

#include <string>

/// OptNode
///     Represents optional types
class OptNode : public ASTNode {
    public:

    private:
        /// type
        ///     The type that is contained inside the opt
        std::string type;
};


#endif
