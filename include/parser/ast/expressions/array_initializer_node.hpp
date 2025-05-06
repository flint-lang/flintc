#pragma once

#include "expression_node.hpp"
#include "parser/type/array_type.hpp"

#include <memory>
#include <vector>

/// @class `ArrayInitializerNode`
/// @brief Represents array initializer expressions
class ArrayInitializerNode : public ExpressionNode {
  public:
    ArrayInitializerNode(                                                 //
        const std::shared_ptr<Type> &type,                                //
        std::vector<std::unique_ptr<ExpressionNode>> &length_expressions, //
        std::optional<std::unique_ptr<ExpressionNode>> &initializer_value //
        ) :
        length_expressions(std::move(length_expressions)),
        initializer_value(std::move(initializer_value)) {
        const ArrayType *arr_type = dynamic_cast<const ArrayType *>(type.get());
        assert(arr_type != nullptr);
        element_type = arr_type->type;
        this->type = type;
    }

    /// @var `element_type`
    /// @brief The type of a single array element
    std::shared_ptr<Type> element_type;

    /// @var `length_expressions`
    /// @brief The expressions of all the dimension lengths
    std::vector<std::unique_ptr<ExpressionNode>> length_expressions;

    /// @var `initializer_value`
    /// @brief The initial value with which all array elements are being initialized with, if nullopt they are initialized with their
    /// default values
    std::optional<std::unique_ptr<ExpressionNode>> initializer_value;
};
