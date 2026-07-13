#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefObjectExtendedTypeUnknown : public BaseError {
  public:
    ErrDefObjectExtendedTypeUnknown( //
        const ErrorType error_type,  //
        const Hash &file_file,       //
        const unsigned int line,     //
        const unsigned int column,   //
        const unsigned int length    //
        ) :
        BaseError(error_type, file_file, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Unknown type found in extends clausel";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Unknown type found in extends clausel";
        return d;
    }
};
