#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/scope.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// @class `DoWhileNode`
/// @brief Represents do-while loops
class DoWhileNode : public StatementNode {
  public:
    DoWhileNode(                                    //
        const Hash &hash,                           //
        const token_slice &tokens,                  //
        std::unique_ptr<ExpressionNode> &condition, //
        std::shared_ptr<Scope> &scope               //
        ) :
        StatementNode(hash, tokens),
        condition(std::move(condition)),
        scope(std::move(scope)) {}

    Variation get_variation() const override {
        return Variation::DO_WHILE;
    }

    // constructor
    DoWhileNode() = delete;
    // deconstructor
    ~DoWhileNode() override = default;
    // copy operations - deleted because of unique_ptr member
    DoWhileNode(const DoWhileNode &) = delete;
    DoWhileNode &operator=(const DoWhileNode &) = delete;
    // move operations
    DoWhileNode(DoWhileNode &&) = default;
    DoWhileNode &operator=(DoWhileNode &&) = default;

    /// @var `condition`
    /// @brief The condition expression
    std::unique_ptr<ExpressionNode> condition;

    /// @var `body`
    /// @brief The body of the while loop
    std::shared_ptr<Scope> scope;
};
