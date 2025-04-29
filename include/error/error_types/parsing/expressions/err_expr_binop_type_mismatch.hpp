#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "lexer/lexer_utils.hpp"
#include "types.hpp"

class ErrExprBinopTypeMismatch : public BaseError {
  public:
    ErrExprBinopTypeMismatch(          //
        const ErrorType error_type,    //
        const std::string &file,       //
        const token_slice &lhs_tokens, //
        const token_slice &rhs_tokens, //
        const Token &operator_token,   //
        const std::string &lhs_type,   //
        const std::string &rhs_type    //
        ) :
        BaseError(error_type, file, lhs_tokens.first->line, lhs_tokens.first->column),
        lhs_tokens(lhs_tokens),
        rhs_tokens(rhs_tokens),
        operator_token(operator_token),
        lhs_type(lhs_type),
        rhs_type(rhs_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Type mismatch in binary expression:\n -- Type of " << YELLOW << get_token_string(lhs_tokens, {})
            << DEFAULT << " doesn't match the type of " << YELLOW << get_token_string(rhs_tokens, {}) << DEFAULT
            << "\n -- LHS type: " << YELLOW << lhs_type << DEFAULT << "\n -- RHS type: " << YELLOW << rhs_type << DEFAULT
            << "\n -- Operator: " << YELLOW << get_token_name(operator_token) << DEFAULT;
        return oss.str();
    }

  private:
    token_slice lhs_tokens;
    token_slice rhs_tokens;
    Token operator_token;
    std::string lhs_type;
    std::string rhs_type;
};
