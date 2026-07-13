#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefObjectExtendedTypeNotObject : public BaseError {
  public:
    ErrDefObjectExtendedTypeNotObject( //
        const ErrorType error_type,    //
        const Hash &file_file,         //
        const unsigned int line,       //
        const unsigned int column,     //
        const unsigned int length      //
        ) :
        BaseError(error_type, file_file, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Only object types are allowed to be extended by other objects";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Only object types are allowed to be extended by other objects";
        return d;
    }
};
