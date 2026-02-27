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
        std::unique_ptr<ExpressionNode> &initializer_value                //
        ) :
        length_expressions(std::move(length_expressions)),
        initializer_value(std::move(initializer_value)) {
        const auto *arr_type = type->as<ArrayType>();
        element_type = arr_type->type;
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::ARRAY_INITIALIZER;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::vector<std::unique_ptr<ExpressionNode>> cloned_length_exprs;
        for (auto &expr : length_expressions) {
            cloned_length_exprs.emplace_back(expr->clone(scope_id));
        }
        std::unique_ptr<ExpressionNode> init_value = initializer_value->clone(scope_id);
        return std::make_unique<ArrayInitializerNode>(this->type, cloned_length_exprs, init_value);
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
    std::unique_ptr<ExpressionNode> initializer_value;
};
