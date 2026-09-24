#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrUnexpectedTokenNumber : public BaseError {
  public:
    ErrUnexpectedTokenNumber(       //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const int line,             //
        const int column,           //
        const char &text            //
        ) :
        BaseError(error_type, file_hash, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Expected number after '" << YELLOW << "." << DEFAULT;
        oss << "' but got '" << YELLOW << text << DEFAULT << "'.\n";
        oss << "└─ Floating point numbers have the form '" << CYAN << "3.14" << DEFAULT << "' for example";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expected number after '.' but got " + std::string(1, text);
        return d;
    }

  private:
    char text;
};
