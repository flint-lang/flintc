#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

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
        BaseError(error_type, file, line, column),
        expected(expected),
        but_got(but_got) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Unexpected token, expected one of [" << YELLOW << get_token_string(expected) << DEFAULT
            << "] but got '" << YELLOW << but_got << DEFAULT << "'";
        return oss.str();
    }

  private:
    std::vector<Token> expected;
    Token but_got;
};
