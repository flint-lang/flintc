#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefEntityNoData : public BaseError {
  public:
    ErrDefEntityNoData(             //
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
        oss << BaseError::to_string() << "├─ Entity type does not operate on any data\n";
        oss << "└─ Use func modules or direct functions when there is no state to operate on";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Entity type does not operate on any data";
        return d;
    }
};
