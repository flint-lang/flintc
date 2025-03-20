#pragma once

#include "parser/ast/ast_node.hpp"
#include "parser/ast/scope.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @class `FunctionNode`
/// @brief Represents function definitions
class FunctionNode : public ASTNode {
  public:
    explicit FunctionNode(                                                   //
        bool is_aligned,                                                     //
        bool is_const,                                                       //
        std::string name,                                                    //
        std::vector<std::tuple<std::string, std::string, bool>> &parameters, //
        std::vector<std::string> &return_types,                              //
        std::unique_ptr<Scope> &scope                                        //
        ) :
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

    /// @var `is_aligned`
    /// @brief Determines whether the function needs to be aligned
    bool is_aligned{false};

    /// @var `is_const`
    /// @brief Determines whether the function is const, e.g. it cannot access data outise of its arguments
    bool is_const{false};

    /// @var `name`
    /// @brief The name of the function
    std::string name;

    /// @var `parameters`
    /// @brief Parameter types, names and whether the parameter variable is mutable
    std::vector<std::tuple<std::string, std::string, bool>> parameters;

    /// @var `return_types`
    /// @brief The types of all return values
    std::vector<std::string> return_types;

    /// @var `scope`
    /// @brief The scope of the function containing all statements
    std::unique_ptr<Scope> scope;
};
