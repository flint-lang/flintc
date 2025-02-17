#ifndef __ERR_EXPR_TYPE_MISMATCH_HPP__
#define __ERR_EXPR_TYPE_MISMATCH_HPP__

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrExprTypeMismatch : public BaseError {
  public:
    ErrExprTypeMismatch(const ErrorType error_type, const std::string &file, const token_list &tokens, const std::string &expected,
        const std::string &type) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens),
        expected(expected),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Type mismatch of expression " << YELLOW << get_token_string(tokens, {}) << DEFAULT
            << "\n -- Expected " << YELLOW << expected << DEFAULT << " but got " << YELLOW << type << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
    std::string expected;
    std::string type;
};

#endif
