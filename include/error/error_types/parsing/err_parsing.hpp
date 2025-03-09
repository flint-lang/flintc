#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrParsing : public BaseError {
  public:
    ErrParsing(const ErrorType error_type, const std::string &file, int line, int column, const token_list &tokens) :
        BaseError(error_type, file, line, column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Offending tokens: ";
        for (const auto &token : tokens) {
            oss << token.lexme << " ";
        }
        return oss.str();
    }

  private:
    token_list tokens;
};
