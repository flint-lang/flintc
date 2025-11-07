#pragma once

#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/scope.hpp"
#include "parser/type/type.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @class `FunctionNode`
/// @brief Represents function definitions
class FunctionNode : public DefinitionNode {
  public:
    explicit FunctionNode(                                                             //
        const std::string &file_name,                                                  //
        const unsigned int line,                                                       //
        const unsigned int column,                                                     //
        const unsigned int length,                                                     //
        bool is_aligned,                                                               //
        bool is_const,                                                                 //
        std::string name,                                                              //
        std::vector<std::tuple<std::shared_ptr<Type>, std::string, bool>> &parameters, //
        std::vector<std::shared_ptr<Type>> &return_types,                              //
        std::vector<std::shared_ptr<Type>> &error_types,                               //
        std::optional<std::shared_ptr<Scope>> &scope                                   //
        ) :
        DefinitionNode(file_name, line, column, length),
        is_aligned(is_aligned),
        is_const(is_const),
        name(std::move(name)),
        parameters(std::move(parameters)),
        return_types(std::move(return_types)),
        error_types(std::move(error_types)),
        scope(std::move(scope)) {}

    Variation get_variation() const override {
        return Variation::FUNCTION;
    }

    // empty constructor
    FunctionNode() = delete;
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
    bool is_aligned;

    /// @var `is_const`
    /// @brief Determines whether the function is const, e.g. it cannot access data outise of its arguments
    bool is_const;

    /// @var `is_extern`
    /// @brief Whether the function is defined externally in FIP
    bool is_extern{false};

    /// @var `name`
    /// @brief The name of the function
    std::string name;

    /// @var `parameters`
    /// @brief Parameter types, names and whether the parameter variable is mutable
    std::vector<std::tuple<std::shared_ptr<Type>, std::string, bool>> parameters;

    /// @var `return_types`
    /// @brief The types of all return values
    std::vector<std::shared_ptr<Type>> return_types;

    /// @var `error_types`
    /// @brief The types of errors this function can throw
    std::vector<std::shared_ptr<Type>> error_types;

    /// @var `scope`
    /// @brief The scope of the function containing all statements or nullopt if the function is just a definition
    std::optional<std::shared_ptr<Scope>> scope;
};
