#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnDefMissing : public BaseError {
  public:
    ErrFnDefMissing(                //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file_hash, tokens),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Function definition is missing the " << YELLOW << "def" << DEFAULT << " keyword\n";
        oss << "└─ Define it like this instead: ";
        auto it = tokens.first;
        while (it != tokens.second && it->token != TOK_LEFT_PAREN) {
            switch (it->token) {
                default:
                    UNREACHABLE();
                    break;
                case TOK_EXTERN:
                    oss << "extern ";
                    break;
                case TOK_CONST:
                    oss << "const ";
                    break;
                case TOK_IDENTIFIER:
                    oss << BLUE << "def " << DEFAULT;
                    oss << it->lexme << "(...):";
                    break;
            }
            it++;
        }
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Function definition is missing the 'def' keyword";
        return d;
    }

  private:
    token_slice tokens;
};
