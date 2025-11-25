#pragma once

#include "error/error_types/base_error.hpp"

class ErrLitUnterminatedString : public BaseError {
  public:
    ErrLitUnterminatedString(       //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const int line,             //
        const int column            //
        ) :
        BaseError(error_type, file_hash, line, column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "The string literal was opened but never closed!";
        return oss.str();
    }
};
