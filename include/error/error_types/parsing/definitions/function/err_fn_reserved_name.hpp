#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnReservedName : public BaseError {
  public:
    ErrFnReservedName(              //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const token_slice &tokens,  //
        const std::string &name     //
        ) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, name.size()),
        name(name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ The function name '" << YELLOW << name << DEFAULT << "' is reserved";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Reserved function name '" + name + "'";
        return d;
    }

  private:
    std::string name;
};
