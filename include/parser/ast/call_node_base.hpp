#pragma once

#include "expressions/expression_node.hpp"

#include <memory>
#include <string>
#include <vector>

/// CallNodeBase
///     The base class for calls, both statements and expressions
class CallNodeBase {
  public:
    CallNodeBase(std::string function_name, std::vector<std::unique_ptr<ExpressionNode>> arguments, std::string type) :
        function_name(std::move(function_name)),
        arguments(std::move(arguments)),
        type(type) {}

    // deconstructor
    ~CallNodeBase() = default;
    // copy operations - disabled due to unique_ptr member
    CallNodeBase(const CallNodeBase &) = delete;
    CallNodeBase &operator=(const CallNodeBase &) = delete;
    // move operations
    CallNodeBase(CallNodeBase &&) = default;
    CallNodeBase &operator=(CallNodeBase &&) = delete;

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
    /// type
    ///     The type of the call`s return value
    std::string type;

  protected:
    CallNodeBase() = default;

  private:
    /// get_next_call_id
    ///     Returns the next call id. Ensures that each call gets its own id for the lifetime of the program
    static unsigned inline int get_next_call_id() {
        static unsigned int call_id = 0;
        return call_id++;
    }
};
