#pragma once

#include "expression_node.hpp"
#include "parser/type/array_type.hpp"

#include <memory>
#include <vector>

/// @class `InlineArrayInitializerNode`
/// @brief Represents inline array initializer expressions like T[N]{...}
class InlineArrayInitializerNode : public ExpressionNode {
  public:
    InlineArrayInitializerNode(                                           //
        const Hash &hash,                                                 //
        const PosTriple &pos,                                             //
        const std::shared_ptr<Type> &type,                                //
        std::vector<std::unique_ptr<ExpressionNode>> &length_expressions, //
        std::vector<std::unique_ptr<ExpressionNode>> &initializer_values  //
        ) :
        ExpressionNode(hash, pos, true),
        length_expressions(std::move(length_expressions)),
        initializer_values(std::move(initializer_values)) {
        const auto *arr_type = type->as<ArrayType>();
        element_type = arr_type->type;
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::INLINE_ARRAY_INITIALIZER;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::vector<std::unique_ptr<ExpressionNode>> cloned_length_exprs;
        for (auto &expr : length_expressions) {
            cloned_length_exprs.emplace_back(expr->clone(scope_id));
        }
        std::vector<std::unique_ptr<ExpressionNode>> cloned_initializer_values;
        for (auto &expr : initializer_values) {
            cloned_initializer_values.emplace_back(expr->clone(scope_id));
        }
        return std::make_unique<InlineArrayInitializerNode>(                                                 //
            file_hash, PosTriple{line, column, length}, type, cloned_length_exprs, cloned_initializer_values //
        );
    }

    /// @var `element_type`
    /// @brief The type of a single array element
    std::shared_ptr<Type> element_type;

    /// @var `length_expressions`
    /// @brief The expressions of all the dimension lengths
    std::vector<std::unique_ptr<ExpressionNode>> length_expressions;

    /// @var `initializer_values`
    /// @brief The initial values with which all array elements are being initialized with
    std::vector<std::unique_ptr<ExpressionNode>> initializer_values;
};
