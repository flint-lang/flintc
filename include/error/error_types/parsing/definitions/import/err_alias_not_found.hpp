#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrAliasNotFound : public BaseError {
  public:
    ErrAliasNotFound(const ErrorType error_type, const std::string &file, unsigned int line, unsigned int column,
        const std::string &alias) :
        BaseError(error_type, file, line, column),
        alias(alias) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "The alias '" << YELLOW << alias << DEFAULT << "' was not defined in this file";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The alias '" + alias + "' was not defined in this file";
        return d;
    }

  private:
    std::string alias;
};
