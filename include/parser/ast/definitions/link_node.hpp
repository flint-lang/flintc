#pragma once

#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/definitions/func_node.hpp"

// Forward-declaration of function reference node
class FunctionReferenceNode;

/// @class `LinkNode`
/// @brief Represents links within entities which link one function to another function
class LinkNode : public DefinitionNode {
  public:
    explicit LinkNode(                               //
        const Hash &file_hash,                       //
        const unsigned int line,                     //
        const unsigned int column,                   //
        const unsigned int length,                   //
        std::unique_ptr<FunctionReferenceNode> &src, //
        std::unique_ptr<FunctionReferenceNode> &dest //
    );

    ~LinkNode() override;
    LinkNode(LinkNode &&);
    LinkNode &operator=(LinkNode &&);

    Variation get_variation() const override {
        return Variation::LINK;
    }

    /// @var `src`
    /// @brief The source function reference of the link
    std::unique_ptr<FunctionReferenceNode> src;

    // @var `dest`
    // @brief The destination function reference of the link
    std::unique_ptr<FunctionReferenceNode> dest;
};
