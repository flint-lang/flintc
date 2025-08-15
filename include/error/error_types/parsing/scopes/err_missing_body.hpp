#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrMissingBody : public BaseError {
  public:
    ErrMissingBody(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(error_type, file, tokens.first->line, (tokens.second - 1)->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Expected a body after the " << YELLOW << ":" << DEFAULT << " from the line above";
        return oss.str();
    }

  private:
    token_slice tokens;
};
