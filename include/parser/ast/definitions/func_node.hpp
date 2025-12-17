#pragma once

#include "function_node.hpp"
#include "parser/ast/definitions/definition_node.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// FuncNode
///     Represents func module definitions
class FuncNode : public DefinitionNode {
  public:
    explicit FuncNode(                                                   //
        const Hash &file_hash,                                           //
        const unsigned int line,                                         //
        const unsigned int column,                                       //
        const unsigned int length,                                       //
        std::string &name,                                               //
        std::vector<std::pair<std::string, std::string>> &required_data, //
        std::vector<std::unique_ptr<FunctionNode>> functions             //
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

    /// name
    ///     The name of the func module
    std::string name;
    /// required_data
    ///     The data types that are required by the func and their variable names
    std::vector<std::pair<std::string, std::string>> required_data;
    /// functions
    ///     The functions defined inside the func module. These functions get passed the required data as arguments by
    ///     default
    std::vector<std::unique_ptr<FunctionNode>> functions;
};
