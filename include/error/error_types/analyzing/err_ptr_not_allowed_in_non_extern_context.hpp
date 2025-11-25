#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/expressions/expression_node.hpp"

class ErrPtrNotAllowedInNonExternContext : public BaseError {
  public:
    ErrPtrNotAllowedInNonExternContext(const ErrorType error_type, const ExpressionNode *expr) :
        BaseError(error_type, expr->file_hash, expr->line, expr->column, expr->length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Pointer types are not allowed in non-extern contexts\n";
        oss << "└─ A pointer type 'T*' can only be used when defining or calling 'extern' functions";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Pointer types are not allowed in non-extern contexts";
        return d;
    }
};
