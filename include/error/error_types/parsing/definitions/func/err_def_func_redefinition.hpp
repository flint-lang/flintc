#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefFuncRedefinition : public BaseError {
  public:
    ErrDefFuncRedefinition(          //
        const ErrorType error_type,  //
        const Hash &file_file,       //
        const unsigned int line,     //
        const unsigned int column,   //
        const std::string &func_name //
        ) :
        BaseError(error_type, file_file, line, column, func_name.size()),
        func_name(func_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Redefinition of func module: " << YELLOW << func_name << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Redefinition of func module '" + func_name + "'";
        return d;
    }

  private:
    const std::string func_name;
};
