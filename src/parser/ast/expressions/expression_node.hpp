#ifndef __EXPRESSION_NODE_HPP__
#define __EXPRESSION_NODE_HPP__

#include "../ast_node.hpp"

#include <string>

/// ExpressionNode
///     Base class for all expressions
class ExpressionNode : public ASTNode {
    public:
        // constructor
        ExpressionNode() = default;
        // deconstructor
        ~ExpressionNode() override = default;
        // copy operations
        ExpressionNode(const ExpressionNode &) = default;
        ExpressionNode& operator=(const ExpressionNode &) = default;
        // move operations
        ExpressionNode(ExpressionNode &&) = default;
        ExpressionNode& operator=(ExpressionNode &&) = default;
    private:
        std::string result_type;
};;

#endif
