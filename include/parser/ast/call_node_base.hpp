#pragma once

#include "expressions/expression_node.hpp"
#include "parser/type/type.hpp"

#include <memory>
#include <vector>

/// @class `CallNodeBase`
/// @brief The base class for calls, both statements and expressions
class CallNodeBase {
  public:
    CallNodeBase(                                                                 //
        FunctionNode *function,                                                   //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type                                         //
        ) :
        function(function),
        arguments(std::move(arguments)),
        error_types(error_types),
        type(type) {}

    // deconstructor
    ~CallNodeBase() = default;
    // copy operations - disabled due to unique_ptr member
    CallNodeBase(const CallNodeBase &) = delete;
    CallNodeBase &operator=(const CallNodeBase &) = delete;
    // move operations
    CallNodeBase(CallNodeBase &&) = default;
    CallNodeBase &operator=(CallNodeBase &&) = delete;

    /// @var `function`
    /// @brief The function being called
    FunctionNode *function;

    /// @var `arguments`
    /// @brief The list of arguments of the function call and whether each argument is a reference or not
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> arguments;

    /// @var `error_types`
    /// @brief The possible error types this function could throw. If this list is empty the function cannot throw at all. All user-defined
    /// functions can throw, but not all Core functions can
    std::vector<std::shared_ptr<Type>> error_types;

    /// @var `type`
    /// @brief The type of the call`s return value(s)
    std::shared_ptr<Type> type;

    /// @var `scope_id`
    /// @brief The id of the scope the call happens in
    unsigned int scope_id{0};

    /// @var `has_catch`
    /// @brief Whether a catch block will follow or not
    bool has_catch{false};

    /// @var `call_id`
    /// @brief The id of this function call. Used for Catch-referentiation of this CallNode
    const unsigned int call_id = get_next_call_id();

  protected:
    CallNodeBase() = default;

  private:
    /// function `get_next_call_id`
    /// @brief Returns the next call id. Ensures that each call gets its own id for the lifetime of the program
    ///
    /// @return `unsigned int` The next unique call id
    static inline unsigned int get_next_call_id() {
        static unsigned int call_id = 0;
        static std::mutex call_id_mutex;
        std::lock_guard<std::mutex> lock(call_id_mutex);
        return call_id++;
    }
};
