#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprInitializerFieldWrongFormat : public BaseError {
  public:
    ErrExprInitializerFieldWrongFormat( //
        const ErrorType error_type,     //
        const Hash &file_hash,          //
        const PosTriple &pos            //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Incorrect field initializer format detected\n";
        oss << "└─ Expected form: " << BLUE << ".value = expression" << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Incorrect field initializer format detected";
        return d;
    }
};
