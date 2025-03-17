#pragma once

#include "parser/ast/statements/statement_node.hpp"

#include <memory>
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
        clone_variable_types(parent);
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
    bool clone_variable_types(const Scope *other) {
        for (const auto &[name, type_scope] : other->variable_types) {
            if (!add_variable_type(name, type_scope.first, type_scope.second)) {
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
    /// @return `bool` Whether insertion of the variable type was successful. If not, this means a variable is shadowed
    bool add_variable_type(const std::string &var, const std::string &type, const unsigned int sid) {
        return variable_types.insert({var, {type, sid}}).second;
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

    /// @var `variable_types`
    /// @brief All the variable types visible within this scope
    ///     The key is the name of the variable
    ///     The value is the type of the variable and the scope it was declared in
    std::unordered_map<std::string, std::pair<std::string, unsigned int>> variable_types;

  private:
    /// @function `get_next_scope_id`
    /// @brief Returns the next scope id. Ensures that each scope gets its own id for the lifetime of the program
    static unsigned inline int get_next_scope_id() {
        static unsigned int scope_id = 0;
        return scope_id++;
    }
};
