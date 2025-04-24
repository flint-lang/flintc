#pragma once

#include "expression_node.hpp"

#include <memory>
#include <vector>

/// @class `ArrayAccessNode`
/// @brief Represents array accesses
class ArrayAccessNode : public ExpressionNode {
  public:
    ArrayAccessNode(                                                       //
        std::shared_ptr<Type> result_type,                                 //
        std::string variable_name,                                         //
        std::shared_ptr<Type> variable_type,                               //
        std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
        ) :
        variable_name(variable_name),
        variable_type(variable_type),
        indexing_expressions(std::move(indexing_expressions)) {
        this->type = result_type;
    }

    /// @var `variable_name`
    /// @brief The name of the variable which is being accessed
    std::string variable_name;

    /// @var `variable_type`
    /// @brief The type of the variable to access
    std::shared_ptr<Type> variable_type;

    /// @var `length_expressions`
    /// @brief The expressions of all the dimension lengths
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;
};
