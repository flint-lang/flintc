#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrInvalidIdentifier : public BaseError {
  public:
    ErrInvalidIdentifier(const ErrorType error_type, const std::string &file, int line, int column, const std::string &identifier) :
        BaseError(error_type, file, line, column),
        identifier(identifier) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Invalid identifier '" << YELLOW << identifier << DEFAULT << "'\n";
        oss << "└─ The prefixes '" << CYAN << "__flint_" << DEFAULT << "' and '" << CYAN << "__fip_" << DEFAULT
            << "' are reserved, nothing you define is allowed to start with them";
        return oss.str();
    }

  private:
    std::string identifier;
};
