#ifndef __ERR_USE_STATEMENT_NOT_AT_TOP_LEVEL_HPP__
#define __ERR_USE_STATEMENT_NOT_AT_TOP_LEVEL_HPP__

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
            << "\n -- Expected: '";
        std::string token_str;
        auto it = tokens.begin();
        for (; it != tokens.end(); it++) {
            if (it->type == TOK_EOL) {
                break;
            }
            if (it->type == TOK_INDENT) {
                continue;
            }
            token_str += it->lexme + " ";
        }
        oss << trim_right(token_str) << "' but got '";
        token_str = "";
        for (it = tokens.begin(); it != tokens.end(); it++) {
            if (it->type == TOK_EOL) {
                break;
            }
            token_str += it->lexme + " ";
        }
        oss << trim_right(token_str) << "'";
        return oss.str();
    }

  private:
    token_list tokens;
};

#endif
