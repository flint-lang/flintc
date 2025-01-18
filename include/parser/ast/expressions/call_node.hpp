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
    explicit CallNode(std::string &function_name, std::vector<std::unique_ptr<ExpressionNode>> &arguments, const std::string &type) :
        function_name(function_name),
        arguments(std::move(arguments)) {
        this->type = type;
    }

    // empty constructor
    CallNode() = default;
    // deconstructor
    ~CallNode() override = default;
    // copy operations - disabled due to unique_ptr member
    CallNode(const CallNode &) = delete;
    CallNode &operator=(const CallNode &) = delete;
    // move operations
    CallNode(CallNode &&) = default;
    CallNode &operator=(CallNode &&) = delete;

    /// function_name
    ///     The name of the function being called
    std::string function_name;
    /// arguments
    ///     The list of arguments of the function call
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    /// scope_id
    ///     The id of the scope the call happens in
    unsigned int scope_id{0};
    /// has_catch
    ///     Determines whether a catch block will follow or not
    bool has_catch{false};
    /// call_id
    ///     The id of this function call. Used for Catch-referentiation of this CallNode
    const unsigned int call_id = get_next_call_id();

  private:
    /// get_next_call_id
    ///     Returns the next call id. Ensures that each call gets its own id for the lifetime of the program
    static unsigned int get_next_call_id() {
        static unsigned int call_id = 0;
        return call_id++;
    }
};

#endif
