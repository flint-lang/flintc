#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrExprCallOfUndefinedFunction : public BaseError {
  public:
    ErrExprCallOfUndefinedFunction(const ErrorType error_type, const std::string &file, const token_slice &tokens,
        const std::string &function_name) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column),
        tokens(tokens),
        function_name(function_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Failed to call function '" << YELLOW << function_name << DEFAULT << "' in expression " << YELLOW
            << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_slice tokens;
    std::string function_name;
};
