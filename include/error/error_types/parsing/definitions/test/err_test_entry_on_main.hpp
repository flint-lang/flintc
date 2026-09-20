#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrTestEntryOnMain : public BaseError {
  public:
    ErrTestEntryOnMain(             //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const unsigned int line,    //
        const unsigned int column,  //
        const unsigned int length   //
        ) :
        BaseError(error_type, file_hash, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ The '" << YELLOW << "#test_entry" << DEFAULT << "' annotation is not allowed on '" << YELLOW
            << "main" << DEFAULT << "' function\n";
        oss << "└─ The 'main' function is skipped in '--test' mode";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The '#test_entry' annotation is not allowed on 'main' function";
        return d;
    }
};
