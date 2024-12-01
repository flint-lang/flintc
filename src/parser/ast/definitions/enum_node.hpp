#ifndef __ENUM_NODE_HPP__
#define __ENUM_NODE_HPP__

#include "../ast_node.hpp"

#include <vector>
#include <string>

/// EnumNode
///     Represents enum type definitions
class EnumNode : public ASTNode {
    public:

    private:
        /// values
        ///     The string values of the enum. Their index in this vector (or their index in the declaration) directly relates to their enum id.
        std::vector<std::string> values;
};

#endif
