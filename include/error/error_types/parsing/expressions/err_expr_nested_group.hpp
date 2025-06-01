#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprNestedGroup : public BaseError {
  public:
    ErrExprNestedGroup(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Nested groups are not allowed: " << YELLOW << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_slice tokens;
};
