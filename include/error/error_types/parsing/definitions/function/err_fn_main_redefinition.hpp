#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnMainRedefinition : public BaseError {
  public:
    ErrFnMainRedefinition(          //
        const ErrorType error_type, //
        const std::string &file,    //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, 4) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Redefinition of the '" << YELLOW << "main" << DEFAULT << "' function\n";
        oss << "└─ Each project is only allowed to contain a single main function";
        return oss.str();
    }
};
