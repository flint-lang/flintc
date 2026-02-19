#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrMissingBody : public BaseError {
  public:
    ErrMissingBody(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(error_type, file_hash, (tokens.first - 1)->line, (tokens.first - 2)->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Expected a body after the " << YELLOW << ":" << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expected a body after the :";
        return d;
    }

  private:
    token_slice tokens;
};
