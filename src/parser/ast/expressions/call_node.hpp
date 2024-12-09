#ifndef __CALL_NODE_HPP__
#define __CALL_NODE_HPP__

#include "expression_node.hpp"

#include <string>
#include <vector>
#include <utility>
#include <memory>

/// CallNode
///     Represents function or method calls
class CallNode : public ExpressionNode {
    public:
        CallNode(std::string &function_name, std::vector<std::unique_ptr<ExpressionNode>> &arguments)
        : function_name(function_name), arguments(std::move(arguments)) {}

        // empty constructor
        CallNode() = default;
        // deconstructor
        ~CallNode() override = default;
        // copy operations - disabled due to unique_ptr member
        CallNode(const CallNode &) = delete;
        CallNode& operator=(const CallNode &) = delete;
        // move operations
        CallNode(CallNode &&) = default;
        CallNode& operator=(CallNode &&) = default;
    private:
        std::string function_name;
        std::vector<std::unique_ptr<ExpressionNode>> arguments;
};

#endif
