#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprInitializerTooManyValues : public BaseError {
  public:
    ErrExprInitializerTooManyValues(   //
        const ErrorType error_type,    //
        const Hash &file_hash,         //
        const PosTriple &pos,          //
        const unsigned int field_count //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        field_count(field_count) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Expected atmost '" << YELLOW << field_count << DEFAULT
            << "' values for initializer, but more were provided";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expected atmost '" + std::to_string(field_count) + "' values for initializer";
        return d;
    }

  private:
    unsigned int field_count;
};
