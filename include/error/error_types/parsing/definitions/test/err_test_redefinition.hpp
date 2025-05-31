#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrTestRedefinition : public BaseError {
  public:
    ErrTestRedefinition(const ErrorType error_type, const std::string &file, unsigned int line, unsigned int column,
        const std::string &test_name) :
        BaseError(error_type, file, line, column),
        test_name(test_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "The test '" << YELLOW << test_name << DEFAULT
            << "' is already defined in this file. Every test must have a unique name within a single file.";
        return oss.str();
    }

  private:
    std::string test_name;
};
