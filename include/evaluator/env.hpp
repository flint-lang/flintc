#pragma once

#include "value/value.hpp"
#include <stack>

/// @struct `Env`
/// @brief The compile-time environment when evaluating a function call
struct Env {
    /// @enum `Mode`
    /// @brief The current mode of the environment evaluation
    enum class Mode {
        // The regular evaluation mode, so just continue evaluating
        EVAL,
        // The mode the evaluator switches to after a return statement is parsed, to notify that the current call needs to end
        RETURN,
        // The mode the evaluator switches to after successfully evaluating a compile-time-only if statement to notify the "outside" that
        // the scope of the if needs to be inlined into the outer scope
        IF,
        // The mode the evaluator switches to when reaching a 'break' statement to break out of the loop / switch it is currently in
        BREAK,
        // The mode the evaluator switches to when reaching a 'continue' statement to continue to the next iteration of a loop
        CONTINUE,
    };

    /// @var `lvalue_stack`
    /// @brief A stack of all lvalue references needed for the evaluator
    std::stack<std::shared_ptr<Value> *> lvalue_stack{};

    /// @var `result`
    /// @brief The result of evaluating something at comptime
    std::optional<std::shared_ptr<Value>> result{std::nullopt};

    /// @var `mode`
    /// @brief The current mode of the evaluation environment
    Mode mode{Mode::EVAL};

    /// @function `declare`
    /// @brief Binds a comptime value to a name in this environment
    ///
    /// @param `name` The name to bind
    /// @param `value` The value to bind the name to
    void declare(const std::string &name, const std::shared_ptr<Value> &value) {
        ASSERT(variables.find(name) == variables.end());
        variables.emplace(name, value);
    }

    /// @function `assign`
    /// @brief Rebinds an already-declared name to a new value
    ///
    /// @param `name` The name to rebind
    /// @param `value` The new value the name will be bound to
    void assign(const std::string &name, const std::shared_ptr<Value> &value) {
        const auto it = variables.find(name);
        if (it != variables.end()) {
            it->second = value;
        }
    }

    /// @function `lookup`
    /// @brief Looks up a comptime value by name, returns nullopt if no variable with the given name was found
    ///
    /// @param `name` The name to get the bound comptime value of
    /// @return `std::optional<std::shared_ptr<Value> *>` The reference to the value bound to the name, nullopt if no variable with the
    /// givne name was found
    ///
    /// @attention The returned pointer is only valid for a very short lifetime, make sure to use it as quickly as possible after the call
    [[nodiscard]] std::optional<std::shared_ptr<Value> *> lookup(const std::string &name) {
        const auto it = variables.find(name);
        if (it == variables.end()) {
            return std::nullopt;
        }
        return &it->second;
    }

  private:
    /// @var `variables`
    /// @brief All comptime values bound in this environment
    std::unordered_map<std::string, std::shared_ptr<Value>> variables{};
};
