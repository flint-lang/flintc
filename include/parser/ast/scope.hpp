#ifndef __ISCOPE_HPP__
#define __ISCOPE_HPP__

#include "error/error.hpp"
#include "types.hpp"

#include <string>
#include <unordered_map>

class Scope {
  public:
    Scope() = default;
    explicit Scope(std::vector<body_statement> body, Scope *parent = nullptr) :
        body(std::move(body)),
        parent_scope(parent) {}

    /// get_parent
    ///     Returns the parent scope of this scope
    Scope *get_parent() const {
        return parent_scope;
    }

    /// set_parent
    ///     Sets the parent scope of this scope
    void set_parent(Scope *parent) {
        parent_scope = parent;
    }

    /// clone_mutations
    ///     Clones the variable reference list of the other scope and also clones their mutable states
    ///     Returns if the cloning was successful
    bool clone_mutations(const Scope *other) {
        for (const auto &mutation : other->variable_mutations) {
            if (!add_variable(mutation.first)) {
                // Duplicate definition / shadowing
                throw_err(ERR_SCOPE);
                return false;
            }
            set_variable_mutated(mutation.first, mutation.second);
        }
        return true;
    }

    /// add_variable
    ///     Adds the given string variable to the list of variable mutations
    ///     Returns true when the variable is "new" and returns false when the variable was already contained in the map
    bool add_variable(const std::string &var) {
        return variable_mutations.insert({var, false}).second;
    }

    /// was_variable_mutated
    ///     Checks if the given variable was mutated.
    ///     Returns false if the variable is not contained in the list, or if it has not been mutated
    ///     Returns true if the variable is contained in the list and has been mutated
    bool was_variable_mutated(const std::string &var) {
        auto var_it = variable_mutations.find(var);
        return var_it != variable_mutations.end() && var_it->second;
    }

    /// set_variable_mutated
    ///     Sets the mutated state of the given variable
    void set_variable_mutated(const std::string &var, bool mutated = true) {
        variable_mutations[var] = mutated;
    }

    /// add_variable_type
    ///     Adds the given variable and its type to the list of variable types
    bool add_variable_type(const std::string &var, const std::string &type) {
        return variable_types.insert({var, type}).second;
    }

    std::vector<body_statement> body;
    Scope *parent_scope{nullptr};
    std::unordered_map<std::string, bool> variable_mutations;
    std::unordered_map<std::string, std::string> variable_types;
};

#endif
