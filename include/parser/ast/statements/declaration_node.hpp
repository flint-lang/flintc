#pragma once

#include "../expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>

/// DeclarationNode
///     Represents variable or data declarations
class DeclarationNode : public StatementNode {
  public:
    DeclarationNode(std::string &type, std::string &name, std::optional<std::unique_ptr<ExpressionNode>> &initializer) :
        type(type),
        name(name),
        initializer(std::move(initializer)) {}

    // empty constructor
    DeclarationNode() = default;
    // destructor
    ~DeclarationNode() override = default;
    // copy operations - disabled due to unique_ptr member
    DeclarationNode(const DeclarationNode &) = delete;
    DeclarationNode &operator=(const DeclarationNode &) = delete;
    // move operations
    DeclarationNode(DeclarationNode &&) = default;
    DeclarationNode &operator=(DeclarationNode &&) = default;

    /// type
    ///     The type of the variable
    std::string type;
    /// name
    ///     The name of the variable
    std::string name;
    /// initializer
    ///     The initial value
    std::optional<std::unique_ptr<ExpressionNode>> initializer;
};
