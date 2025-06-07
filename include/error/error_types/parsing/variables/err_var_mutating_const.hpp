#pragma once

#include "error/error_types/base_error.hpp"

class ErrVarMutatingConst : public BaseError {
  public:
    ErrVarMutatingConst(const ErrorType error_type, const std::string &file, int line, int column, const std::string &var_name) :
        BaseError(error_type, file, line, column),
        var_name(var_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Variable '" + var_name + "' is marked as 'const' and cannot be modified!";
        return oss.str();
    }

  private:
    std::string var_name;
};
