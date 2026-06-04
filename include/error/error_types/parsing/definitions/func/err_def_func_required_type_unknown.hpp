#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefFuncRequiredTypeUnknown : public BaseError {
  public:
    ErrDefFuncRequiredTypeUnknown(  //
        const ErrorType error_type, //
        const Hash &file_file,      //
        const unsigned int line,    //
        const unsigned int column,  //
        const unsigned int length   //
        ) :
        BaseError(error_type, file_file, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Unknown type found in requires clausel";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Unknown type found in requires clausel";
        return d;
    }
};
