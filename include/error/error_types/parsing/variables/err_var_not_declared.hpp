#pragma once

#include "error/error_types/base_error.hpp"

class ErrVarNotDeclared : public BaseError {
  public:
    ErrVarNotDeclared(const ErrorType error_type, const std::string &file, int line, int column, const std::string &var_name) :
        BaseError(error_type, file, line, column),
        var_name(var_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Use of undeclared variable '" + var_name + "'";
        return oss.str();
    }

  private:
    std::string var_name;
};
