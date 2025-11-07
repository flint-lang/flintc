#pragma once

#include "expression_node.hpp"

#include <string>

/// @class `DataAccessNode`
/// @brief Represents the accessing of a single datas value
class DataAccessNode : public ExpressionNode {
  public:
    DataAccessNode(                                   //
        std::unique_ptr<ExpressionNode> &base_expr,   //
        const std::optional<std::string> &field_name, //
        const unsigned int field_id,                  //
        const std::shared_ptr<Type> &field_type       //
        ) :
        base_expr(std::move(base_expr)),
        field_name(field_name),
        field_id(field_id) {
        this->type = field_type;
    }

    Variation get_variation() const override {
        return Variation::DATA_ACCESS;
    }

    /// @var `base_expr`
    /// @brief The base expression from which to access one field's value
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `field_name`
    /// @brief The name of the accessed field, if the accessed field has no name it means its accessed via `.$N` instead, for tuples or
    /// multi-types
    std::optional<std::string> field_name;

    /// @var `field_id`
    /// @brief The index of the field in the data
    unsigned int field_id;
};
