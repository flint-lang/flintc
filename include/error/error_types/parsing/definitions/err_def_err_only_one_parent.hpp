#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrDefErrOnlyOneParent : public BaseError {
  public:
    ErrDefErrOnlyOneParent(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(                                                   //
            error_type,                                              //
            file,                                                    //
            tokens.first->line,                                      //
            (tokens.first + 3)->column,                              //
            (tokens.second - 2)->column - (tokens.first + 3)->column //
        ) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Error sets can only extend from a single other error set";
        return oss.str();
    }
};
