#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrTestEntryInvalid : public BaseError {
  public:
    ErrTestEntryInvalid(              //
        const ErrorType error_type,   //
        const Hash &file_hash,        //
        const unsigned int line,      //
        const unsigned int column,    //
        const unsigned int length     //
        ) :
        BaseError(error_type, file_hash, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Invalid definition of the '" << YELLOW << "#test_entry" << DEFAULT << "' function\n";
        oss << "└─ The '#test_entry' function has to take one 'str[]' parameter and can only return 'i32' or nothing (void)";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Invalid definition of the '#test_entry' function";
        return d;
    }
};