#ifndef __FUNCTION_NODE_HPP__
#define __FUNCTION_NODE_HPP__

#include "parser/ast/ast_node.hpp"
#include "parser/ast/scope.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// FunctionNode
///     Represents function definitions
class FunctionNode : public ASTNode {
  public:
    explicit FunctionNode(bool is_aligned, bool is_const, std::string name, std::vector<std::pair<std::string, std::string>> &parameters,
        std::vector<std::string> &return_types, std::unique_ptr<Scope> &scope) :
        is_aligned(is_aligned),
        is_const(is_const),
        name(std::move(name)),
        parameters(std::move(parameters)),
        return_types(std::move(return_types)),
        scope(std::move(scope)) {}

    // empty constructor
    FunctionNode() = default;
    // deconstructor
    ~FunctionNode() override = default;
    // copy operations - disabled due to unique_ptr member
    FunctionNode(const FunctionNode &) = delete;
    FunctionNode &operator=(const FunctionNode &) = delete;
    // move operations
    FunctionNode(FunctionNode &&) = default;
    FunctionNode &operator=(FunctionNode &&) = default;

    /// is_aligned
    ///     Determines whether the function needs to be aligned
    bool is_aligned{false};
    /// is_const
    ///     Determines whether the function is const, e.g. it cannot access data outise of its arguments
    bool is_const{false};
    /// name
    ///     The name of the function
    std::string name;
    /// parameters
    ///     Parameter names and types
    std::vector<std::pair<std::string, std::string>> parameters;
    /// return_types
    ///     The types of all return values
    std::vector<std::string> return_types;
    /// body
    ///     The body of the function containing all statements
    std::unique_ptr<Scope> scope;
};

#endif
