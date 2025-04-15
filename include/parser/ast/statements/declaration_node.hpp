#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>

/// @class `DeclarationNode`
/// @brief Represents variable or data declarations
class DeclarationNode : public StatementNode {
  public:
    DeclarationNode(std::shared_ptr<Type> &type, std::string &name, std::optional<std::unique_ptr<ExpressionNode>> &initializer) :
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

    /// @var `type`
    /// @brief The type of the variable
    std::shared_ptr<Type> type;

    /// @var `name`
    /// @brief The name of the variable
    std::string name;

    /// @var `initializer`
    /// @brief The initial value
    std::optional<std::unique_ptr<ExpressionNode>> initializer;
};
