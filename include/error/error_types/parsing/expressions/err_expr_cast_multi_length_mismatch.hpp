#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"

class ErrExprCastMultiLengthMismatch : public BaseError {
  public:
    ErrExprCastMultiLengthMismatch(        //
        const ErrorType error_type,        //
        const Hash &file_hash,             //
        const token_slice &tokens,         //
        const unsigned int expected_count, //
        const unsigned int actual_count    //
        ) :
        BaseError(error_type, file_hash, tokens),
        expected_count(expected_count),
        actual_count(actual_count) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Expected '" << std::to_string(expected_count) << "' values inside multi-type cast but got '"
            << std::to_string(actual_count) << "' values instead";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expected '" + std::to_string(expected_count) + "' values inside multi - type cast but got '" +
            std::to_string(actual_count) + "' values instead";
        return d;
    }

  private:
    unsigned int expected_count;
    unsigned int actual_count;
};
