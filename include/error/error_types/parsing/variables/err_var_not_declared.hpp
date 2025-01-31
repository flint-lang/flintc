#ifndef __ERR_VAR_NOT_DECLARED_HPP__
#define __ERR_VAR_NOT_DECLARED_HPP__

#include "error/error_types/base_error.hpp"

class ErrVarNotDeclared : public BaseError {
  public:
    ErrVarNotDeclared(const std::string &message, const std::string &file, int line, int column, const std::string &var_name) :
        BaseError(file, line, column, message),
        var_name(var_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "\n -- Use of undeclared variable '" + var_name + "'!";
        return oss.str();
    }

  private:
    std::string var_name;
};

#endif
