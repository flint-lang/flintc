#pragma once

#include "expression_node.hpp"
#include "parser/type/optional_type.hpp"
#include "resolver/resolver.hpp"

/// @class `VariantExtractionNode`
/// @brief Represents variant extractions
class VariantExtractionNode : public ExpressionNode {
  public:
    VariantExtractionNode(                          //
        const Hash &hash,                           //
        std::unique_ptr<ExpressionNode> &base_expr, //
        const std::shared_ptr<Type> &extracted_type //
        ) :
        ExpressionNode(hash),
        base_expr(std::move(base_expr)),
        extracted_type(extracted_type) {
        this->type = std::make_shared<OptionalType>(extracted_type);
        Namespace *file_namespace = Resolver::get_namespace_from_hash(file_hash);
        if (!file_namespace->add_type(this->type)) {
            this->type = file_namespace->get_type_from_str(this->type->to_string()).value();
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
