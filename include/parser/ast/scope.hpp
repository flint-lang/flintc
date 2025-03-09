#pragma once

#include "parser/ast/statements/statement_node.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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

    /// get_parent
    ///     Returns the parent scope of this scope
    [[nodiscard]]
    Scope *get_parent() const {
        return parent_scope;
    }

    /// set_parent
    ///     Sets the parent scope of this scope
    void set_parent(Scope *parent) {
        parent_scope = parent;
    }

    /// clone_variable_types
    ///     Clones the variable types from the other scope
    ///     Returns if the cloning was successful
    bool clone_variable_types(const Scope *other) {
        for (const auto &[name, type_scope] : other->variable_types) {
            if (!add_variable_type(name, type_scope.first, type_scope.second)) {
                // Duplicate definition / shadowing
                return false;
            }
        }
        return true;
    }

    /// add_variable_type
    ///     Adds the given variable and its type to the list of variable types
    bool add_variable_type(const std::string &var, const std::string &type, const unsigned int sid) {
        return variable_types.insert({var, {type, sid}}).second;
    }

    /// scope_id
    ///     The unique id of this scope. Every scope has its own id
    const unsigned int scope_id = get_next_scope_id();
    /// body
    ///     All the body statements of this scopes body
    std::vector<std::unique_ptr<StatementNode>> body;
    /// parent_scope
    ///     The parent scope of this scope
    Scope *parent_scope{nullptr};
    /// variable_types
    ///     The key is the name of the variable
    ///     The value is the type of the variable and the scope it was declared in
    std::unordered_map<std::string, std::pair<std::string, unsigned int>> variable_types;

  private:
    /// get_next_scope_id
    ///     Returns the next scope id. Ensures that each scope gets its own id for the lifetime of the program
    static unsigned inline int get_next_scope_id() {
        static unsigned int scope_id = 0;
        return scope_id++;
    }
};
