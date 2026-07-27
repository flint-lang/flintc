#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefObjectProvidedTypeNotData : public BaseError {
  public:
    ErrDefObjectProvidedTypeNotData( //
        const ErrorType error_type,  //
        const Hash &file_hash,       //
        const token_slice &tokens    //
        ) :
        BaseError(error_type, file_hash, tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Type mismatch. Expected data type for object-provided data";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Type mismatch. Expected data type for object-provided data";
        return d;
    }
};
