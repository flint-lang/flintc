#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnMainInvalid : public BaseError {
  public:
    ErrFnMainInvalid(               //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file_hash, tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Invalid variation of the main function\n";
        oss << "└─ The main function can only be one of the following variations:\n";
        oss << "    ├─ " << CYAN << "main()" << DEFAULT << "\n";
        oss << "    ├─ " << CYAN << "main(str[] args)" << DEFAULT << "\n";
        oss << "    ├─ " << CYAN << "main() -> i32" << DEFAULT << "\n";
        oss << "    └─ " << CYAN << "main(str[] args) -> i32" << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Invalid variation of the main function";
        return d;
    }
};
