#pragma once

#include "function_node.hpp"
#include "parser/ast/definitions/definition_node.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @class `FuncNode`
/// @brief Represents func module definitions
class FuncNode : public DefinitionNode {
  public:
    explicit FuncNode(                                                             //
        const Hash &file_hash,                                                     //
        const unsigned int line,                                                   //
        const unsigned int column,                                                 //
        const unsigned int length,                                                 //
        const std::string &name,                                                   //
        std::vector<std::pair<std::shared_ptr<Type>, std::string>> &required_data, //
        std::vector<FunctionNode *> &functions                                     //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        name(name),
        required_data(std::move(required_data)),
        functions(std::move(functions)) {}

    Variation get_variation() const override {
        return Variation::FUNC;
    }

    // destructor
    ~FuncNode() override = default;
    // copy operations - disabled due to unique_ptr member
    FuncNode(const FuncNode &) = delete;
    FuncNode &operator=(const FuncNode &) = delete;
    // move operations
    FuncNode(FuncNode &&) = default;
    FuncNode &operator=(FuncNode &&) = default;

    /// @var `name`
    /// @brief The name of the func module
    std::string name;

    /// @var `required_data`
    /// @brief The data types that are required by the func and their accessor names
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> required_data;

    /// @var `functions`
    /// @brief The functions defined inside the func module. These functions get passed the required data as arguments by default
    std::vector<FunctionNode *> functions;
};
