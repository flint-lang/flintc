#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprFnRefNonexistent : public BaseError {
  public:
    ErrExprFnRefNonexistent(             //
        const ErrorType error_type,      //
        const Hash &file_hash,           //
        const token_slice &tokens,       //
        const std::string &function_name //
        ) :
        BaseError(error_type, file_hash, tokens),
        function_name(function_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Referencing undefined function '" << YELLOW << function_name << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Referencing undefined function '" + function_name + "'";
        return d;
    }

  private:
    std::string function_name;
};
