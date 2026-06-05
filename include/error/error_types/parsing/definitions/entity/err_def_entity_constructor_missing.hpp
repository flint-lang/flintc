#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefEntityConstructorMissing : public BaseError {
  public:
    ErrDefEntityConstructorMissing( //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const unsigned int line,    //
        const unsigned int column,  //
        const unsigned int length   //
        ) :
        BaseError(error_type, file_hash, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Missing constructor in entity type definition";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Missing constructor in entity type definition";
        return d;
    }
};
