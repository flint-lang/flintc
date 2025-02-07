#ifndef __ERR_USE_STATEMENT_NOT_AT_TOP_LEVEL_HPP__
#define __ERR_USE_STATEMENT_NOT_AT_TOP_LEVEL_HPP__

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "lexer/token.hpp"
#include "types.hpp"

class ErrUseStatementNotAtTopLevel : public BaseError {
  public:
    ErrUseStatementNotAtTopLevel(const ErrorType error_type, const std::string &file, int line, int column, const token_list &tokens) :
        BaseError(error_type, file, line, column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "The use statement was not at the top level."
            << "\n -- Expected " << YELLOW << get_token_string(tokens, {TOK_INDENT}) << DEFAULT << " but got " << YELLOW
            << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
};

#endif
