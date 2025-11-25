#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrStmtDanglingCatch : public BaseError {
  public:
    ErrStmtDanglingCatch(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Dangling catch statement without preceeding function call";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Dangling catch statement without preceeding function call";
        return d;
    }

  private:
    token_slice tokens;
};
