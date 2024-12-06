#ifndef __VARIANT_NODE_HPP__
#define __VARIANT_NODE_HPP__

#include "../ast_node.hpp"

#include <vector>
#include <string>
#include <utility>

/// VariantNode
///     Represents the variant type definition
class VariantNode : public ASTNode {
    public:
        explicit VariantNode(std::string &name, std::vector<std::string> &possible_types)
        : name(name), possible_types(std::move(possible_types)) {}
    private:
        /// name
        ///     The name of the variant type
        std::string name;
        /// possible_types
        ///     List of all types the varaint can hold
        std::vector<std::string> possible_types;
};

#endif
