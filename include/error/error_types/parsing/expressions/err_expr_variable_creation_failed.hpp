#ifndef __ERR_EXPR_VARIABLE_CREATION_FAILED_HPP__
#define __ERR_EXPR_VARIABLE_CREATION_FAILED_HPP__

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrExprVariableCreationFailed : public BaseError {
  public:
    ErrExprVariableCreationFailed(const ErrorType error_type, const std::string &file, const token_list &tokens) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Failed to parse variable: " << YELLOW << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
};

#endif
