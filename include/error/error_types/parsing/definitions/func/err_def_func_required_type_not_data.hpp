#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefFuncRequiredTypeNotData : public BaseError {
  public:
    ErrDefFuncRequiredTypeNotData(  //
        const ErrorType error_type, //
        const Hash &file_file,      //
        const size_t line,          //
        const size_t column,        //
        const size_t length         //
        ) :
        BaseError(error_type, file_file, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Only data types are allowed to be required by func modules";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Only data types are allowed to be required by func modules";
        return d;
    }
};
