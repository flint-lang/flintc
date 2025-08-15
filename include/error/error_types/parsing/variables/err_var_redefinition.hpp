#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrVarRedefinition : public BaseError {
  public:
    ErrVarRedefinition(const ErrorType error_type, const std::string &file, int line, int column, const std::string &var_name) :
        BaseError(error_type, file, line, column, var_name.size()),
        var_name(var_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Variable '" << YELLOW << var_name << DEFAULT << "' already exists";
        return oss.str();
    }

  private:
    std::string var_name;
};
