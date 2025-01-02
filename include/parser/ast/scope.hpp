#ifndef __ISCOPE_HPP__
#define __ISCOPE_HPP__

#include "types.hpp"

#include <string>
#include <unordered_map>

class Scope {
  public:
    explicit Scope(std::vector<body_statement> body, Scope *parent = nullptr) :
        body(std::move(body)),
        parent_scope(parent) {}

    Scope *get_parent() const {
        return parent_scope;
    }

    void set_parent(Scope *parent) {
        parent_scope = parent;
    }

    bool add_variable(const std::string &var) {
        return variable_mutations.insert({var, false}).second;
    }

    bool was_variable_mutated(const std::string &var) {
        auto var_it = variable_mutations.find(var);
        return var_it != variable_mutations.end() && var_it->second;
    }

    void set_variable_mutated(const std::string &var, bool mutated = true) {
        variable_mutations[var] = mutated;
    }

    std::vector<body_statement> body;
    Scope *parent_scope{nullptr};
    std::unordered_map<std::string, bool> variable_mutations;
};

#endif
