#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"

class ErrExternFnNotFound : public BaseError {
  public:
    ErrExternFnNotFound(               //
        const ErrorType error_type,    //
        const FunctionNode *missing_fn //
        ) :
        BaseError(error_type, missing_fn->file_hash, missing_fn->line, missing_fn->column, missing_fn->length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Extern function could not be found in any FIP module";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Extern function could not be found in any FIP module";
        return d;
    }
};
