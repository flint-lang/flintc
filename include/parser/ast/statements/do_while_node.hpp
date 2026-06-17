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
    DoWhileNode(const Hash &file_hash, const unsigned int line, const unsigned int column, const unsigned int length,
                std::unique_ptr<ExpressionNode> &condition, std::shared_ptr<Scope> &scope) :
        condition(std::move(condition)),
        scope(std::move(scope)) {
        this->file_hash = file_hash;
        this->line = line;
        this->column = column;
        this->length = length;
    }

    Variation get_variation() const override {
        return Variation::DO_WHILE;
    }

    // constructor
    DoWhileNode() = default;
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
