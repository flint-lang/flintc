#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrMissingSemicolon : public BaseError {
  public:
    ErrMissingSemicolon(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(                                                        //
            error_type,                                                   //
            file_hash,                                                    //
            (tokens.second - 1)->line,                                    //
            (tokens.second - 1)->column                                   //
                + ((tokens.second - 1)->token == TOK_TYPE                 //
                          ? (tokens.second - 1)->type->to_string().size() //
                          : (tokens.second - 1)->lexme.size())            //
                - 1                                                       //
            ),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Expected a " << YELLOW << ";" << DEFAULT << " as the terminator of the statement";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expected a ; as the terminator of the statement";
        return d;
    }

  private:
    token_slice tokens;
};
