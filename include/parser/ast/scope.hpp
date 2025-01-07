#ifndef __ISCOPE_HPP__
#define __ISCOPE_HPP__

#include "error/error.hpp"
#include "types.hpp"

#include <string>
#include <unordered_map>

class Scope {
  public:
    Scope() = default;

    explicit Scope(Scope *parent) :
        parent_scope(parent) {
        clone_variable_types(parent);
    }

    explicit Scope(std::vector<body_statement> body, Scope *parent = nullptr) :
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
        for (const auto &type : other->variable_types) {
            if (!add_variable_type(type.first, type.second)) {
                // Duplicate definition / shadowing
                throw_err(ERR_SCOPE);
                return false;
            }
        }
        return true;
    }

    /// add_variable_type
    ///     Adds the given variable and its type to the list of variable types
    bool add_variable_type(const std::string &var, const std::string &type) {
        return variable_types.insert({var, type}).second;
    }

    std::vector<body_statement> body;
    Scope *parent_scope{nullptr};
    std::unordered_map<std::string, std::string> variable_types;
};

#endif
