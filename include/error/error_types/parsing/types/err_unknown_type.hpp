#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrUnknownType : public BaseError {
  public:
    ErrUnknownType(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ This type is unknown, maybe a typo or missing import?";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Unknown Type";
        return d;
    }
};
