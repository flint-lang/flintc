#ifndef __CALL_NODE_HPP__
#define __CALL_NODE_HPP__

#include "expression_node.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// CallNode
///     Represents function or method calls
class CallNode : public ExpressionNode {
  public:
    CallNode(std::string &function_name, std::vector<std::unique_ptr<ExpressionNode>> &arguments) :
        function_name(function_name),
        arguments(std::move(arguments)) {}

    // empty constructor
    CallNode() = default;
    // deconstructor
    ~CallNode() override = default;
    // copy operations - disabled due to unique_ptr member
    CallNode(const CallNode &) = delete;
    CallNode &operator=(const CallNode &) = delete;
    // move operations
    CallNode(CallNode &&) = default;
    CallNode &operator=(CallNode &&) = default;

    /// function_name
    ///     The name of the function being called
    std::string function_name;
    /// arguments
    ///     The list of arguments of the function call
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    /// scope_id
    ///     The id of the scope the call happens in
    unsigned int scope_id{0};
};

#endif
