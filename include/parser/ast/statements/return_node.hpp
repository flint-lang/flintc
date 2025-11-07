#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// class `ReturnNode`
/// @brief Represents return statements
class ReturnNode : public StatementNode {
  public:
    explicit ReturnNode(std::optional<std::unique_ptr<ExpressionNode>> &return_value) :
        return_value(std::move(return_value)) {}

    Variation get_variation() const override {
        return Variation::RETURN;
    }

    // constructor
    ReturnNode() = default;
    // deconstructor
    ~ReturnNode() override = default;
    // copy operations - deleted because of unique_ptr member
    ReturnNode(const ReturnNode &) = delete;
    ReturnNode &operator=(const ReturnNode &) = delete;
    // move operations
    ReturnNode(ReturnNode &&) = default;
    ReturnNode &operator=(ReturnNode &&) = default;

    /// @var `return_value`
    /// @brief The return values expression, nullopt if the function doesnt return anything
    std::optional<std::unique_ptr<ExpressionNode>> return_value;
};
