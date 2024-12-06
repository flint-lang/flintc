#ifndef __CALL_NODE_HPP__
#define __CALL_NODE_HPP__

#include "expression_node.hpp"

#include <string>
#include <vector>
#include <utility>

/// CallNode
///     Represents function or method calls
class CallNode : public ExpressionNode {
    public:
        CallNode(std::string &function_name, std::vector<ExpressionNode> &arguments)
        : function_name(function_name), arguments(std::move(arguments)) {}
    private:
        std::string function_name;
        std::vector<ExpressionNode> arguments;
};

#endif
