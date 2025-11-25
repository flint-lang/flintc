#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrVarNotDeclared : public BaseError {
  public:
    ErrVarNotDeclared(              //
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
        oss << BaseError::to_string() << "└─ Use of undeclared variable '" << YELLOW << var_name << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Use of undeclared variable '" + var_name + "'";
        return d;
    }

  private:
    std::string var_name;
};
