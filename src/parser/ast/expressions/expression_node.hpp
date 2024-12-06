#ifndef __EXPRESSION_NODE_HPP__
#define __EXPRESSION_NODE_HPP__

#include "../ast_node.hpp"

#include <string>

/// ExpressionNode
///     Base class for all expressions
class ExpressionNode : public ASTNode {
    public:
        ExpressionNode(const ExpressionNode &) = default;
        ExpressionNode(ExpressionNode &&) = delete;
        ExpressionNode &operator=(const ExpressionNode &) = default;
        ExpressionNode &operator=(ExpressionNode &&) = delete;
        virtual ~ExpressionNode() = default;
    private:
        std::string result_type;
};

#endif
