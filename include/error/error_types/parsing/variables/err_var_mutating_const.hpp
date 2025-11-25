#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrVarMutatingConst : public BaseError {
  public:
    ErrVarMutatingConst(            //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const int line,             //
        const int column,           //
        const std::string &var_name //
        ) :
        BaseError(error_type, file_hash, line, column, var_name.size()),
        var_name(var_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Variable '" << YELLOW << var_name << DEFAULT << "' is marked as '" << YELLOW << "const"
            << DEFAULT << "' and cannot be modified!";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Variable '" + var_name + "' is marked as const and cannot be modified";
        return d;
    }

  private:
    std::string var_name;
};
