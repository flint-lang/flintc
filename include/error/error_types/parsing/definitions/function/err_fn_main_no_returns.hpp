#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnMainNoReturns : public BaseError {
  public:
    ErrFnMainNoReturns(             //
        const ErrorType error_type, //
        const std::string &file,    //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ The main function is not allwed to return any value\n";
        oss << "└─ The main function can only be one of the following variations:\n";
        oss << "    ├─ " << CYAN << "main()" << DEFAULT << "\n";
        oss << "    └─ " << CYAN << "main(str[] args)" << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The main function is not allwed to return any value";
        return d;
    }
};
