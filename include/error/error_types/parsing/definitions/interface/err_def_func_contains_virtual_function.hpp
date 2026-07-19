#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefInterfaceContainsConcreteFunction : public BaseError {
  public:
    ErrDefInterfaceContainsConcreteFunction( //
        const ErrorType error_type,          //
        const Hash &file_file,               //
        const token_slice &tokens            //
        ) :
        BaseError(error_type, file_file, tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Interfaces are only allowed to contain virtual functions\n";
        oss << "└─ The function is not allowed to have a body";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Interfaces are only allowed to contain virtual functions";
        return d;
    }
};
