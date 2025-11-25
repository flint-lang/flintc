#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrInvalidIdentifier : public BaseError {
  public:
    ErrInvalidIdentifier(             //
        const ErrorType error_type,   //
        const Hash &file_hash,        //
        const int line,               //
        const int column,             //
        const std::string &identifier //
        ) :
        BaseError(error_type, file_hash, line, column),
        identifier(identifier) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Invalid identifier '" << YELLOW << identifier << DEFAULT << "'\n";
        oss << "└─ The prefix '" << CYAN << "__flint_" << DEFAULT << "' is reserved, nothing you define is allowed to start with it";
        return oss.str();
    }

  private:
    std::string identifier;
};
