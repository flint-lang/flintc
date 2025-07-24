#pragma once

#include "expression_node.hpp"

#include <memory>
#include <vector>

/// @class `ArrayAccessNode`
/// @brief Represents array accesses
class ArrayAccessNode : public ExpressionNode {
  public:
    ArrayAccessNode(                                                       //
        std::unique_ptr<ExpressionNode> &base_expr,                        //
        std::shared_ptr<Type> result_type,                                 //
        std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
        ) :
        base_expr(std::move(base_expr)),
        indexing_expressions(std::move(indexing_expressions)) {
        this->type = result_type;
    }

    /// @var `base_expr`
    /// @brief The base expression from which the array elements are accessed
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `indexing_expressions`
    /// @brief The expressions of all the dimensions indices
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;
};
