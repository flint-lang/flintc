#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

#include <variant>

class ErrExprTypeMismatch : public BaseError {
  public:
    ErrExprTypeMismatch(                                                     //
        const ErrorType error_type,                                          //
        const std::string &file,                                             //
        const token_list &tokens,                                            //
        const std::variant<std::string, std::vector<std::string>> &expected, //
        const std::variant<std::string, std::vector<std::string>> &type      //
        ) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens),
        expected(expected),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Type mismatch of expression " << YELLOW << get_token_string(tokens, {}) << DEFAULT
            << "\n -- Expected " << YELLOW << get_type_string(expected) << DEFAULT << " but got " << YELLOW << get_type_string(type)
            << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
    std::variant<std::string, std::vector<std::string>> expected;
    std::variant<std::string, std::vector<std::string>> type;
};
