#ifndef __LINK_NODE_HPP__
#define __LINK_NODE_HPP__

#include "../ast_node.hpp"

#include <string>

/// LinkNode
///     Represents links within entities
class LinkNode : public ASTNode {
    public:

    private:
        /// from
        ///     The function reference of the function that gets shadowed
        std::string from;
        // to
        //      The function reference of the function that gets referenced
        std::string to;
};

#endif
