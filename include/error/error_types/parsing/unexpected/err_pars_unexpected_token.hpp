#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "lexer/lexer_utils.hpp"

class ErrParsUnexpectedToken : public BaseError {
  public:
    ErrParsUnexpectedToken(                 //
        const ErrorType error_type,         //
        const std::string &file,            //
        const unsigned int line,            //
        const unsigned int column,          //
        const std::vector<Token> &expected, //
        const Token &but_got                //
        ) :
        BaseError(error_type, file, line, column, get_token_name(but_got).size()),
        expected(expected),
        but_got(but_got) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Unexpected token. Got " << YELLOW << get_token_name(but_got) << DEFAULT;
        oss << " but expected ";
        if (expected.size() == 1) {
            oss << "" << CYAN << get_token_name(expected.front()) << DEFAULT;
            return oss.str();
        }
        oss << "one of these:\n";
        for (auto it = expected.begin(); it != expected.end(); ++it) {
            if ((it + 1) != expected.end()) {
                oss << "    ├─ ";
            } else {
                oss << "    └─ ";
            }
            oss << CYAN << get_token_name(*it) << DEFAULT;

            if ((it + 1) != expected.end()) {
                oss << "\n";
            }
        }
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Unexpected Token: '" + get_token_name(but_got) + "', expected one of [";
        for (auto it = expected.begin(); it != expected.end(); ++it) {
            d.message.append(get_token_name(*it));
            if ((it + 1) != expected.end()) {
                d.message.append(",");
            }
        }
        d.message.append("]");
        return d;
    }

  private:
    std::vector<Token> expected;
    Token but_got;
};
