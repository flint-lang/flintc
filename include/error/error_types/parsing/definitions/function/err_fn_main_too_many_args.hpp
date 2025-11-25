#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnMainTooManyArgs : public BaseError {
  public:
    ErrFnMainTooManyArgs(           //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Too many arguments provided for the main function\n";
        oss << "└─ The main function can only be one of the following variations:\n";
        oss << "    ├─ " << CYAN << "main()" << DEFAULT << "\n";
        oss << "    └─ " << CYAN << "main(str[] args)" << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Too many arguments in the main function";
        return d;
    }
};
