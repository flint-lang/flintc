#pragma once

#include "expression_node.hpp"
#include "parser/type/optional_type.hpp"

/// @class `VariantExtractionNode`
/// @brief Represents variant extractions
class VariantExtractionNode : public ExpressionNode {
  public:
    VariantExtractionNode(                          //
        std::unique_ptr<ExpressionNode> &base_expr, //
        const std::shared_ptr<Type> &extracted_type //
        ) :
        base_expr(std::move(base_expr)),
        extracted_type(extracted_type) {
        this->type = std::make_shared<OptionalType>(extracted_type);
        if (!Type::add_type(this->type)) {
            this->type = Type::get_type_from_str(this->type->to_string()).value();
        }
    }

    Variation get_variation() const override {
        return Variation::VARIANT_EXTRACTION;
    }

    /// @var `base_expr`
    /// @brief The base expression which is accessed in the optional chain
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `extracted_type`
    /// @brief The type to extract from the variant
    std::shared_ptr<Type> extracted_type;
};
