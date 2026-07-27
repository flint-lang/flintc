#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefObjectNoData : public BaseError {
  public:
    ErrDefObjectNoData(             //
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
        oss << BaseError::to_string() << "├─ Object type does not operate on any data\n";
        oss << "└─ Use direct functions when there is no state to operate on";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Object type does not operate on any data";
        return d;
    }
};
