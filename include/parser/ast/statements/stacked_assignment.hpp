#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>

/// @class `StackedAssignmentNode`
/// @brief Represents stacked assignments to a single data field
class StackedAssignmentNode : public StatementNode {
  public:
    StackedAssignmentNode(                                //
        std::unique_ptr<ExpressionNode> &base_expression, //
        const std::string &field_name,                    //
        const unsigned int field_id,                      //
        const std::shared_ptr<Type> &field_type,          //
        std::unique_ptr<ExpressionNode> &expression       //
        ) :
        base_expression(std::move(base_expression)),
        field_name(field_name),
        field_id(field_id),
        field_type(field_type),
        expression(std::move(expression)) {}

    Variation get_variation() const override {
        return Variation::STACKED_ASSIGNMENT;
    }

    // constructor
    StackedAssignmentNode() = delete;
    // deconstructor
    ~StackedAssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    StackedAssignmentNode(const StackedAssignmentNode &) = delete;
    StackedAssignmentNode &operator=(const StackedAssignmentNode &) = delete;
    // move operations
    StackedAssignmentNode(StackedAssignmentNode &&) = default;
    StackedAssignmentNode &operator=(StackedAssignmentNode &&) = default;

    /// @var `base_expression`
    /// @brief The lhs expression at which the rhs expression is assigned to
    ///
    /// @note If the lhs is `var.field.field = ...` the `base_expression` is `var.field`.
    std::unique_ptr<ExpressionNode> base_expression;

    /// @var `field_name`
    /// @brief The name of the field to assign to
    std::string field_name;

    /// @var `field_id`
    /// @brief The id of the field to assign to
    unsigned int field_id;

    /// @var `field_type`
    /// @brief The type of the field to assign to
    std::shared_ptr<Type> field_type;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
