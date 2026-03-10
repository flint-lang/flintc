#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrDefErrOnlyOneParent : public BaseError {
  public:
    ErrDefErrOnlyOneParent(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(                                                                //
            error_type,                                                           //
            file_hash,                                                            //
            tokens.first->line,                                                   //
            (tokens.first + 3)->column,                                           //
            slice_visual_length(token_slice{tokens.first + 3, tokens.second - 2}) //
        ) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Error sets can only extend from a single other error set";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Error sets can only extend from a single other error set";
        return d;
    }
};
