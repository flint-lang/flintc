#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrTypeTupleMultiTypeOverlap : public BaseError {
  public:
    ErrTypeTupleMultiTypeOverlap(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Cannot create a tuple type which overlaps with a multi-type";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Cannot create a tuple type which overlaps with a multi-type";
        return d;
    }

  private:
    token_slice tokens;
};
