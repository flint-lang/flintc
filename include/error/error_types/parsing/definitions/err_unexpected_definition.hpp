#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrUnexpectedDefinition : public BaseError {
  public:
    ErrUnexpectedDefinition(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ The definition could not be parsed\n";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The definition cannot be parsed";
        return d;
    }
};
