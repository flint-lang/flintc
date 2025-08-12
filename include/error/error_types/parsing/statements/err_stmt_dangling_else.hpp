#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrStmtDanglingElse : public BaseError {
  public:
    ErrStmtDanglingElse(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, tokens.first->lexme.size()),
        is_else_if((tokens.first + 1)->token == TOK_IF) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Dangling else " << (is_else_if ? "if " : "") << "statement misses preceeding if or else if";
        return oss.str();
    }

  private:
    bool is_else_if;
};
