#ifndef __BASE_AST_HPP__
#define __BASE_AST_HPP__

#include "node_type.hpp"

#include <vector>
#include <memory>

/// ASTNode
///     Base class for all AST nodes.
class ASTNode {
    public:

    private:
        NodeType type;
        std::vector<std::shared_ptr<ASTNode>> children;
};

#endif
