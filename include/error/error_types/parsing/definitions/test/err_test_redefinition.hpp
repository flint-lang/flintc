#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrTestRedefinition : public BaseError {
  public:
    ErrTestRedefinition(             //
        const ErrorType error_type,  //
        const Hash &file_hash,       //
        const unsigned int line,     //
        const unsigned int column,   //
        const std::string &test_name //
        ) :
        BaseError(error_type, file_hash, line, column, test_name.size() + 2),
        test_name(test_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ The test '" << YELLOW << test_name << DEFAULT << "' is already defined in this file\n";
        oss << "└─ Each test within a file must have a unique name";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The test '" + test_name + "' is already defined in this file";
        return d;
    }

  private:
    std::string test_name;
};
