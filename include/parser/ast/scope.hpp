#pragma once

#include "parser/ast/statements/statement_node.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/// @class `Scope`
/// @brief This class represents a scope and is responsible for keeping track of all variable declarations
class Scope {
  public:
    Scope() = default;

    explicit Scope(Scope *parent) :
        parent_scope(parent) {
        clone_variables(parent);
    }

    explicit Scope(std::vector<std::unique_ptr<StatementNode>> body, Scope *parent = nullptr) :
        body(std::move(body)),
        parent_scope(parent) {}

    /// @function `get_parent`
    /// @brief Returns the parent scope of this scope
    ///
    /// @return `Scope *` A pointer to the parent of this scope
    [[nodiscard]]
    Scope *get_parent() const {
        return parent_scope;
    }

    /// @function `set_parent`
    /// @brief Sets the parent scope of this scope
    ///
    /// @param `parent` The new parent of this scope
    void set_parent(Scope *parent) {
        parent_scope = parent;
    }

    /// @function `clone_variable_types`
    /// @brief Clones the variable types from the other scope
    ///
    /// @param `other` The other scope from which to clone the variable types
    /// @return `bool` Whether the cloning was successful
    bool clone_variables(const Scope *other) {
        for (const auto &[name, type_scope] : other->variables) {
            if (!add_variable(name, std::get<0>(type_scope), std::get<1>(type_scope), std::get<2>(type_scope))) {
                // Duplicate definition / shadowing
                return false;
            }
        }
        return true;
    }

    /// @function `add_variable_type`
    /// @brief Adds the given variable and its type to the list of variable types
    ///
    /// @param `var` The name of the added variable
    /// @param `type` The type of the added variable
    /// @param `sid` The id of the scope the variable is declared in
    /// @param `is_mutable` Whether the variable is mutable
    /// @return `bool` Whether insertion of the variable type was successful. If not, this means a variable is shadowed
    bool add_variable(const std::string &var, const std::string &type, const unsigned int sid, const bool is_mutable) {
        return variables.insert({var, {type, sid, is_mutable}}).second;
    }

    /// @function `get_variable_type`
    /// @brief Return the type of the given variable name, if it exists
    ///
    /// @param `var_name` The name of the variable to search for
    /// @return `std::optional<std::string>` The type of the variable, or nullopt if the variable doesnt exist
    std::optional<std::string> get_variable_type(const std::string &var_name) {
        if (variables.find(var_name) == variables.end()) {
            return std::nullopt;
        }
        return std::get<0>(variables.at(var_name));
    }

    /// @function `gen_unique_variables`
    /// @brief Returns all variable definitions which are unique to this scope, and not present in the parent scope. This function is used
    /// for easy handling for variables when they go out of scope
    ///
    /// @return `std::unordered_map<std::string, std::tuple<std::string, unsigned int, bool>>` The variable map thats unique to this scope
    std::unordered_map<std::string, std::tuple<std::string, unsigned int, bool>> get_unique_variables() const {
        if (parent_scope == nullptr) {
            return variables;
        }
        auto unique_variables = variables;
        for (const auto &variable : parent_scope->variables) {
            unique_variables.erase(variable.first);
        }
        return unique_variables;
    }

    /// @var `scope_id`
    /// @brief The unique id of this scope. Every scope has its own id
    const unsigned int scope_id = get_next_scope_id();

    /// @var `body`
    /// @brief All the body statements of this scopes body
    std::vector<std::unique_ptr<StatementNode>> body;

    /// @var `parent_scope`
    /// @brief The parent scope of this scope
    Scope *parent_scope{nullptr};

    /// @var `variable`
    /// @brief All the variables visible within this scope
    ///     The key is the name of the variable
    ///     The value is:
    ///         1. the type of the variable
    ///         2. the scope it was declared in
    ///         3. if the variable is mutable
    std::unordered_map<std::string, std::tuple<std::string, unsigned int, bool>> variables;

  private:
    /// @function `get_next_scope_id`
    /// @brief Returns the next scope id. Ensures that each scope gets its own id for the lifetime of the program
    static unsigned inline int get_next_scope_id() {
        static unsigned int scope_id = 0;
        return scope_id++;
    }
};
