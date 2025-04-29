#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrUnclosedParen : public BaseError {
  public:
    ErrUnclosedParen(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Parenthesis opened but never closed: " << YELLOW << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_slice tokens;
};
