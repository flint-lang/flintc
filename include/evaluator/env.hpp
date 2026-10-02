#pragma once

#include "value/value.hpp"
#include <stack>

/// @struct `Env`
/// @brief The compile-time environment when evaluating a function call
struct Env {
    /// @var `lvalue_stack`
    /// @brief A stack of all lvalue references needed for the evaluator
    std::stack<std::shared_ptr<Value> *> lvalue_stack{};

    /// @var `result`
    /// @brief The result of evaluating something at comptime
    std::optional<std::shared_ptr<Value>> result{std::nullopt};

    /// @var `done`
    /// @brief Whether evaluation is done (either through an error or because of a present result)
    bool done{false};

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
