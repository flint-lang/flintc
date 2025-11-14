#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"

class ErrPtrNotAllowedInInternalFunction : public BaseError {
  public:
    ErrPtrNotAllowedInInternalFunction(const ErrorType error_type, const FunctionNode *function) :
        BaseError(error_type, function->file_name, function->line, function->column, function->length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Pointer types are not allowed in non-extern function definitions\n";
        oss << "└─ A pointer type 'T*' can only be used when defining 'extern' functions";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Pointer types are not allowed in non-extern function definition";
        return d;
    }
};
