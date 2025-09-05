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
        BaseError(                                               //
            error_type,                                          //
            file,                                                //
            lhs_tokens.first->line,                              //
            lhs_tokens.first->column,                            //
            rhs_tokens.second->column - lhs_tokens.first->column //
            ),
        operator_token(operator_token),
        lhs_type(lhs_type),
        rhs_type(rhs_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Type mismatch in binary expression. Cannot apply operation " << YELLOW
            << get_token_name(operator_token) << DEFAULT << " on types:\n";
        oss << "│   ├─ LHS type: " << YELLOW << lhs_type << DEFAULT << "\n";
        oss << "│   └─ RHS type: " << YELLOW << rhs_type << DEFAULT << "\n";
        oss << "└─ Have you considered using explicit casting of types?";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "BinOp type mismatch between '" + lhs_type + "' and '" + rhs_type + "'";
        return d;
    }

  private:
    Token operator_token;
    std::string lhs_type;
    std::string rhs_type;
};
