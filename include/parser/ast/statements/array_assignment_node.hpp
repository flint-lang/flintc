#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// @class `ArrayAssignmentNode`
/// @brief Represents array assignments
class ArrayAssignmentNode : public StatementNode {
  public:
    ArrayAssignmentNode(                                                    //
        std::unique_ptr<ExpressionNode> &base_expr,                         //
        std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions, //
        std::unique_ptr<ExpressionNode> &expression                         //
        ) :
        base_expr(std::move(base_expr)),
        indexing_expressions(std::move(indexing_expressions)),
        expression(std::move(expression)) {}

    Variation get_variation() const override {
        return Variation::ARRAY_ASSIGNMENT;
    }

    // constructor
    ArrayAssignmentNode() = default;
    // deconstructor
    ~ArrayAssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    ArrayAssignmentNode(const ArrayAssignmentNode &) = delete;
    ArrayAssignmentNode &operator=(const ArrayAssignmentNode &) = delete;
    // move operations
    ArrayAssignmentNode(ArrayAssignmentNode &&) = default;
    ArrayAssignmentNode &operator=(ArrayAssignmentNode &&) = default;

    /// @var `base_expr`
    /// @brief The base array expression on which to assign the value to
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `indexing_expressions`
    /// @brief The indexing expressions for all indices
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
