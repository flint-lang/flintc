#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrUnexpectedDefinition : public BaseError {
  public:
    ErrUnexpectedDefinition(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ The definition could not be parsed\n";
        return oss.str();
    }
};
