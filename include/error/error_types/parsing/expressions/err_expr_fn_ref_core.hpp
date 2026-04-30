#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprFnRefCore : public BaseError {
  public:
    ErrExprFnRefCore(                    //
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
        oss << BaseError::to_string() << "├─ Referencing Core module function '" << YELLOW << function_name << DEFAULT << "'\n";
        oss << "└─ Referencing functions of Core modules is not allowed";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Referencing Core module function '" + function_name + "'";
        return d;
    }

  private:
    std::string function_name;
};
