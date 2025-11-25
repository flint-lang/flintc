#pragma once

#include "error/error_types/base_error.hpp"

class ErrVarFromRequiresList : public BaseError {
  public:
    ErrVarFromRequiresList(         //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const int line,             //
        const int column,           //
        const std::string &var_name //
        ) :
        BaseError(error_type, file_hash, line, column),
        var_name(var_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Variable '" + var_name + "' is already defined in the 'requires' statement of the func module";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Variable '" + var_name + "' is already defined in the 'requires' statement of the func module";
        return d;
    }

  private:
    std::string var_name;
};
