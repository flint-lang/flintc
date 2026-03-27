#pragma once

#include "parser/ast/statements/statement_node.hpp"
#include "parser/type/type.hpp"

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

    explicit Scope(std::shared_ptr<Scope> parent, const unsigned int parent_scope_segment) :
        parent_scope(parent),
        parent_scope_segment(parent_scope_segment) {
        clone_variables(parent);
    }

    explicit Scope(                                       //
        std::vector<std::unique_ptr<StatementNode>> body, //
        std::shared_ptr<Scope> parent = nullptr,          //
        const unsigned int parent_scope_segment = 0       //
        ) :
        body(std::move(body)),
        parent_scope(parent),
        parent_scope_segment(parent_scope_segment) {}

    /// @type `Variable`
    /// @brief A simple struct contianing all information needed for tracking a variable's declaration
    struct Variable {
        /// @var `type`
        /// @brief The type of the variable
        std::shared_ptr<Type> type;

        /// @var `scope_id`
        /// @brief The scope the variable was declared in
        unsigned int scope_id;

        /// @var `scope_segment`
        /// @brief The segment of the scope the variable was declared in
        unsigned int scope_segment;

        /// @var `is_mutable`
        /// @brief Whether this variable is mutable
        bool is_mutable;

        /// @var `is_fn_param`
        /// @brief Whether this variable is a function parameter
        bool is_fn_param;

        /// @var `is_fn_param`
        /// @brief Whether the variable is a reference to another variable, e.g. does not need to be cleaned up at end of scope
        bool is_reference = false;

        /// @var `return_scope_ids`
        /// @brief A list of all scope id's the variable is returned as it's value, if any
        std::vector<unsigned int> return_scope_ids = {};

        /// @var `is_pseudo_variable`
        /// @brief Whether this variable is a pseudo-variable, for example like the `flint.return_type` variable used to have access to a
        /// function's return type within the generator functions without a reference to the function node itself
        bool is_pseudo_variable = false;

        bool operator==(const Variable &other) const {
            return type->equals(other.type)                   //
                && scope_id == other.scope_id                 //
                && scope_segment == other.scope_segment       //
                && is_mutable == other.is_mutable             //
                && is_fn_param == other.is_fn_param           //
                && is_reference == other.is_reference         //
                && return_scope_ids == other.return_scope_ids //
                && is_pseudo_variable == other.is_pseudo_variable;
        }
    };

    /// @function `get_parent`
    /// @brief Returns the parent scope of this scope
    ///
    /// @return `std::shared_ptr<Scope> ` A pointer to the parent of this scope
    [[nodiscard]]
    std::shared_ptr<Scope> get_parent() const {
        return parent_scope;
    }

    /// @function `set_parent`
    /// @brief Sets the parent scope of this scope
    ///
    /// @param `parent` The new parent of this scope
    void set_parent(std::shared_ptr<Scope> parent) {
        parent_scope = parent;
    }

    /// @function `clone_variable_types`
    /// @brief Clones the variable types from the other scope
    ///
    /// @param `other` The other scope from which to clone the variable types
    /// @return `bool` Whether the cloning was successful
    bool clone_variables(const std::shared_ptr<Scope> other);

    /// @function `add_variable_type`
    /// @brief Adds the given variable and its type to the list of variable types
    ///
    /// @param `var_name` The name of the added variable
    /// @param `variable` The variable to insert
    /// @return `bool` Whether insertion of the variable type was successful. If not, this means a variable is shadowed
    bool add_variable(const std::string &var_name, const Variable &variable);

    /// @function `get_variable_type`
    /// @brief Return the type of the given variable name, if it exists
    ///
    /// @param `var_name` The name of the variable to search for
    /// @return `std::optional<std::shared_ptr<Type>>` The type of the variable, nullopt if the variable doesnt exist
    std::optional<std::shared_ptr<Type>> get_variable_type(const std::string &var_name);

    /// @function `get_unique_variables`
    /// @brief Returns all variable definitions which are unique to this scope, and not present in the parent scope. This function is used
    /// for easy handling for variables when they go out of scope
    ///
    /// @return `std::unordered_map<std::string, Variable>` The variable map thats unique to this scope
    std::unordered_map<std::string, Variable> get_unique_variables(const unsigned int segment) const;

    /// @function `get_all_variables`
    /// @brief Returns all variables from all nested scopes within this scope and it's subscopes
    ///
    /// @return `std::vector<std::pair<std::string, Variable>>` A list of all varaibles of all nested scopes
    std::vector<std::pair<std::string, Variable>> get_all_variables() const;

    /// @var `scope_id`
    /// @brief The unique id of this scope. Every scope has its own id
    const unsigned int scope_id = get_next_scope_id();

    /// @var `body`
    /// @brief All the body statements of this scopes body
    std::vector<std::unique_ptr<StatementNode>> body;

    /// @var `parent_scope`
    /// @brief The parent scope of this scope
    std::shared_ptr<Scope> parent_scope{nullptr};

    /// @var `parent_scope_segment`
    /// @brief The scope segment of the parent scope after which this scope started
    unsigned int parent_scope_segment{0};

    /// @var `variable`
    /// @brief All the variables visible within this scope
    std::unordered_map<std::string, Variable> variables;

  private:
    /// @function `get_next_scope_id`
    /// @brief Returns the next scope id. Ensures that each scope gets its own id for the lifetime of the program
    static unsigned inline int get_next_scope_id() {
        static unsigned int scope_id = 0;
        static std::mutex scope_id_mutex;
        std::lock_guard<std::mutex> lock(scope_id_mutex);
        return scope_id++;
    }
};
