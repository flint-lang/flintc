#pragma once

#include "expressions/expression_node.hpp"
#include "parser/type/type.hpp"

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
        const std::shared_ptr<Type> &type                                        //
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
    std::shared_ptr<Type> type;

  protected:
    CallNodeBase() = default;

  private:
    /// function `get_next_call_id`
    /// @brief Returns the next call id. Ensures that each call gets its own id for the lifetime of the program
    ///
    /// @return `unsigned int` The next unique call id
    static inline unsigned int get_next_call_id() {
        static unsigned int call_id = 0;
        return call_id++;
    }
};
