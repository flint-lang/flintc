#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnMainErrSet : public BaseError {
  public:
    ErrFnMainErrSet(                //
        const ErrorType error_type, //
        const std::string &file,    //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ The main function is not allowed to have any error-set signature specialization";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The main function is not allowed to have any error-set signature specialization";
        return d;
    }
};
