#pragma once

#include "expressions/expression_node.hpp"

#include <memory>
#include <string>
#include <vector>

/// @class `CallNodeBase`
/// @brief The base class for calls, both statements and expressions
class CallNodeBase {
  public:
    CallNodeBase(                                                                //
        std::string function_name,                                               //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> arguments, //
        const std::variant<std::string, std::vector<std::string>> &type          //
        ) :
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

    /// @var `function_name`
    /// @brief The name of the function being called
    std::string function_name;

    /// @var `arguments`
    /// @brief The list of arguments of the function call and whether each argument is a reference or not
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> arguments;

    /// @var `scope_id`
    /// @brief The id of the scope the call happens in
    unsigned int scope_id{0};

    /// @var `has_catch`
    /// @brief Whether a catch block will follow or not
    bool has_catch{false};

    /// @var `call_id`
    /// @brief The id of this function call. Used for Catch-referentiation of this CallNode
    const unsigned int call_id = get_next_call_id();

    /// @var `type`
    /// @brief The type of the call`s return value(s)
    std::variant<std::string, std::vector<std::string>> type;

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
