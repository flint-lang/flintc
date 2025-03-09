#pragma once

#include "error/error_types/base_error.hpp"

class ErrValUnknownLiteral : public BaseError {
  public:
    ErrValUnknownLiteral(const ErrorType error_type, const std::string &file, int line, int column, const std::string &literal_value) :
        BaseError(error_type, file, line, column),
        literal_value(literal_value) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Literal '" + literal_value + "' not parsable";
        return oss.str();
    }

  private:
    std::string literal_value;
};
