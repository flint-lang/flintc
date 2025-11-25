#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrUnexpectedToken : public BaseError {
  public:
    ErrUnexpectedToken(             //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const int line,             //
        const int column,           //
        const std::string &text     //
        ) :
        BaseError(error_type, file_hash, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Unknown character '" << YELLOW << text << DEFAULT << "'";
        return oss.str();
    }

  private:
    std::string text;
};
