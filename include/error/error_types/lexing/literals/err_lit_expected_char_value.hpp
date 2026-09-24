#pragma once

#include "error/error_types/base_error.hpp"

class ErrLitExpectedCharValue : public BaseError {
  public:
    ErrLitExpectedCharValue(        //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const int line,             //
        const int column,           //
        const std::string &text     //
        ) :
        BaseError(error_type, file_hash, line, column, text.size()),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Unknown character '" << text << "' when expecting char value";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Unknown character '" + text + "' when expecting char value";
        return d;
    }

  private:
    std::string text;
};
