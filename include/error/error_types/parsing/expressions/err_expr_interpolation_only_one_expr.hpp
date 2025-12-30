#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprInterpolationOnlyOneExpr : public BaseError {
  public:
    ErrExprInterpolationOnlyOneExpr(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ It is not allowed to interpolate a single expression using " << YELLOW
            << get_token_string(tokens, {}) << DEFAULT << "\n";
        oss << "└─ Use " << BLUE << "str(";
        assert(tokens.first->token == TOK_DOLLAR);
        assert((tokens.first + 1)->token == TOK_STR_VALUE);
        const std::string_view &str_value = (tokens.first + 1)->lexme;
        const std::string_view expr = str_value.substr(1, str_value.size() - 2);
        oss << expr << ")" << DEFAULT << " instead!";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Interpolating a single expression without anything surrounding it is not allowed, use string casting instead";
        return d;
    }

  private:
    const token_slice &tokens;
};
