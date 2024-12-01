#ifndef __VARIANT_NODE_HPP__
#define __VARIANT_NODE_HPP__

#include "../ast_node.hpp"

#include <vector>
#include <string>

/// VariantNode
///     Represents the variant type definition
class VariantNode : public ASTNode {
    public:

    private:
        /// possible_types
        ///     List of all types the varaint can hold
        std::vector<std::string> possible_types;
};

#endif
