#ifndef __STATEMENT_NODE_HPP__
#define __STATEMENT_NODE_HPP__

#include "../ast_node.hpp"

/// StatementNode
///     Base class for all statements
class StatementNode : public ASTNode {
    public:
        StatementNode(const StatementNode &) = default;
        StatementNode(StatementNode &&) = delete;
        StatementNode &operator=(const StatementNode &) = default;
        StatementNode &operator=(StatementNode &&) = delete;
        virtual ~StatementNode() = default;
    private:
};

#endif
