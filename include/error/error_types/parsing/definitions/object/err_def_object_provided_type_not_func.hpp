#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefObjectProvidedTypeNotFunc : public BaseError {
  public:
    ErrDefObjectProvidedTypeNotFunc( //
        const ErrorType error_type,  //
        const Hash &file_hash,       //
        const token_slice &tokens    //
        ) :
        BaseError(error_type, file_hash, tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Included func component is not a func component";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Included func component is not a func component";
        return d;
    }
};
