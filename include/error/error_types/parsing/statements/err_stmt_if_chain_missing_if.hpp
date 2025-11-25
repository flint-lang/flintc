#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrStmtIfChainMissingIf : public BaseError {
  public:
    ErrStmtIfChainMissingIf(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.first->lexme.size()) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Expected if chain to start with an if statement";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expected if chain to start with an if statement";
        return d;
    }
};
