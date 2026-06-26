#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/scope.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// @class `WhileNode`
/// @brief Represents while loops
class WhileNode : public StatementNode {
  public:
    WhileNode(                                      //
        const Hash &hash,                           //
        const token_slice &tokens,                  //
        std::unique_ptr<ExpressionNode> &condition, //
        std::shared_ptr<Scope> &scope               //
        ) :
        StatementNode(hash, tokens),
        condition(std::move(condition)),
        scope(std::move(scope)) {}

    Variation get_variation() const override {
        return Variation::WHILE;
    }

    // constructor
    WhileNode() = delete;
    // deconstructor
    ~WhileNode() override = default;
    // copy operations - deleted because of unique_ptr member
    WhileNode(const WhileNode &) = delete;
    WhileNode &operator=(const WhileNode &) = delete;
    // move operations
    WhileNode(WhileNode &&) = default;
    WhileNode &operator=(WhileNode &&) = default;

    /// @var `condition`
    /// @brief The condition expression
    std::unique_ptr<ExpressionNode> condition;

    /// @var `body`
    /// @brief The body of the while loop
    std::shared_ptr<Scope> scope;
};
