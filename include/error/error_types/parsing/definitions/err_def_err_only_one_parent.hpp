#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrDefErrOnlyOneParent : public BaseError {
  public:
    ErrDefErrOnlyOneParent(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "error set can only extend a single other error set: " << YELLOW << get_token_string(tokens, {})
            << DEFAULT;
        return oss.str();
    }

  private:
    token_slice tokens;
};
