#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefObjectLinkNameMismatch : public BaseError {
  public:
    ErrDefObjectLinkNameMismatch(   //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const token_slice tokens    //
        ) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Linking functions with different names is not allowed";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Linking functions with different names is not allowed";
        return d;
    }
};
