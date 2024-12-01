#ifndef __CALL_NODE_HPP__
#define __CALL_NODE_HPP__

#include "expression_node.hpp"

#include <string>
#include <vector>

/// CallNode
///     Represents function or method calls
class CallNode : ExpressionNode {
    public:

    private:
        std::string function_name;
        std::vector<ExpressionNode> arguments;
};

#endif
