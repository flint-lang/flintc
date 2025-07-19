#pragma once

#include "parser/ast/definitions/variant_node.hpp"
#include "type.hpp"

/// @class `VariantType`
/// @brief Represents variant types
class VariantType : public Type {
  public:
    VariantType(VariantNode *const variant_node) :
        variant_node(variant_node) {}

    std::string to_string() override {
        return variant_node->name;
    }

    /// @var `variant_node`
    /// @brief The variant node this variant type points to
    VariantNode *const variant_node;
};
