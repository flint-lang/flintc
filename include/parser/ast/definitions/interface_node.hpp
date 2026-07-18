#pragma once

#include "function_node.hpp"
#include "parser/ast/definitions/definition_node.hpp"

#include <string>
#include <utility>
#include <vector>

/// @class `InterfaceNode`
/// @brief Represents interface definitions
class InterfaceNode : public DefinitionNode {
  public:
    explicit InterfaceNode(                    //
        const Hash &file_hash,                 //
        const unsigned int line,               //
        const unsigned int column,             //
        const unsigned int length,             //
        const std::string &name,               //
        std::vector<FunctionNode *> &functions //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        name(name),
        functions(std::move(functions)) {}

    Variation get_variation() const override {
        return Variation::INTERFACE;
    }

    // destructor
    ~InterfaceNode() override = default;
    // copy operations - disabled due to unique_ptr member
    InterfaceNode(const InterfaceNode &) = delete;
    InterfaceNode &operator=(const InterfaceNode &) = delete;
    // move operations
    InterfaceNode(InterfaceNode &&) = default;
    InterfaceNode &operator=(InterfaceNode &&) = default;

    /// @var `name`
    /// @brief The name of the interface
    std::string name;

    /// @var `functions`
    /// @brief The functions defined inside the interface. These functions all must be virtual functions
    std::vector<FunctionNode *> functions;
};
