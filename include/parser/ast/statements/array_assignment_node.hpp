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
        const std::string &variable_name,                                   //
        const std::shared_ptr<Type> &array_type,                            //
        const std::shared_ptr<Type> &value_type,                            //
        std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions, //
        std::unique_ptr<ExpressionNode> &expression                         //
        ) :
        variable_name(variable_name),
        array_type(array_type),
        value_type(value_type),
        indexing_expressions(std::move(indexing_expressions)),
        expression(std::move(expression)) {}

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

    /// @var `variable_name`
    /// @brief The name of the array variable to assign to
    std::string variable_name;

    /// @var `array_type`
    /// @brief The type of the accessed array variable
    std::shared_ptr<Type> array_type;

    /// @var `value_type`
    /// @brief The type of the assigned value
    std::shared_ptr<Type> value_type;

    /// @var `indexing_expressions`
    /// @brief The indexing expressions for all indices
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;
};
