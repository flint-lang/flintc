#pragma once

#include "error/error_types/base_error.hpp"

class ErrExprNestedGroup : public BaseError {
  public:
    ErrExprNestedGroup(const ErrorType error_type, const std::string &file, const token_slice &tokens) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Nested groups are not allowed";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Nested groups are not allowed";
        return d;
    }

  private:
};
