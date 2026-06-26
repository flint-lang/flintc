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
    DeclarationNode(                                                //
        const Hash &hash,                                           //
        const token_slice &tokens,                                  //
        const std::shared_ptr<Type> &type,                          //
        const std::string &name,                                    //
        const bool is_persistent,                                   //
        std::optional<std::unique_ptr<ExpressionNode>> &initializer //
        ) :
        StatementNode(hash, tokens),
        type(type),
        name(name),
        is_persistent(is_persistent),
        initializer(std::move(initializer)) {}

    Variation get_variation() const override {
        return Variation::DECLARATION;
    }

    // empty constructor
    DeclarationNode() = delete;
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

    /// @var `is_persistent`
    /// @brief Whether the declared variable is persistent
    bool is_persistent;

    /// @var `persistence_id`
    /// @brief The ID of the persistent variable within a function
    size_t persistence_id{0};

    /// @var `initializer`
    /// @brief The initial value
    std::optional<std::unique_ptr<ExpressionNode>> initializer;
};
