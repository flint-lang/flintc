#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "lexer/token.hpp"
#include "types.hpp"

class ErrUseClauselNotAtTopLevel : public BaseError {
  public:
    ErrUseClauselNotAtTopLevel(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "The use clausel was not at the top level."
            << "\n -- Expected " << YELLOW << get_token_string(tokens, {TOK_INDENT}) << DEFAULT << " but got " << YELLOW
            << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Use clausel not at top level";
        return d;
    }

  private:
    token_slice tokens;
};
