#ifndef __STATEMENT_NODE_HPP__
#define __STATEMENT_NODE_HPP__

#include "../ast_node.hpp"

/// StatementNode
///     Base class for all statements
class StatementNode : public ASTNode {
    public:
        // constructor
        StatementNode() = default;
        // destructor
        ~StatementNode() override = default;
        // copy operations - disabled by default
        StatementNode(const StatementNode &) = delete;
        StatementNode& operator=(const StatementNode &) = delete;
        // move operations
        StatementNode(StatementNode &&) = default;
        StatementNode& operator=(StatementNode &&) = default;
    private:
};

#endif
