#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/variant_node.hpp"

class ErrPtrNotAllowedInVariantDefinition : public BaseError {
  public:
    ErrPtrNotAllowedInVariantDefinition(const ErrorType error_type, const VariantNode *node) :
        BaseError(error_type, node->file_name, node->line, node->column, node->length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Pointer types are not allowed in variant definitions\n";
        oss << "└─ A pointer type 'T*' can only be used when defining or calling 'extern' functions";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Pointer types are not allowed in variant definitions";
        return d;
    }
};
