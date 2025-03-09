#pragma once

#include "error/error_types/base_error.hpp"

class ErrLitExpectedCharValue : public BaseError {
  public:
    ErrLitExpectedCharValue(const ErrorType error_type, const std::string &file, int line, int column, const std::string &text) :
        BaseError(error_type, file, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Unknown character '" << text << "' when expecting char value";
        return oss.str();
    }

  private:
    std::string text;
};
