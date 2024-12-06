#ifndef __BASE_AST_HPP__
#define __BASE_AST_HPP__

#include "node_type.hpp"

#include <vector>

/// ASTNode
///     Base class for all AST nodes.
class ASTNode {
    public:
        // constructor
        ASTNode() = default;
        // destructor
        virtual ~ASTNode() = default;
        // copy operators
        ASTNode(const ASTNode &) = default;
        ASTNode& operator=(const ASTNode &) = default;
        // move operators
        ASTNode(ASTNode &&) = default;
        ASTNode& operator=(ASTNode &&) = default;
    private:
        //NodeType type;
        //std::vector<ASTNode> children;
};

#endif
