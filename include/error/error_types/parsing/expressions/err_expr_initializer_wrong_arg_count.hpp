#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprInitializerWrongArgCount : public BaseError {
  public:
    ErrExprInitializerWrongArgCount(     //
        const ErrorType error_type,      //
        const Hash &file_hash,           //
        const token_slice &tokens,       //
        const size_t expected_arg_count, //
        const size_t actual_arg_count    //
        ) :
        BaseError(error_type, file_hash, tokens),
        expected_arg_count(expected_arg_count),
        actual_arg_count(actual_arg_count) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Expected " << BLUE << std::to_string(expected_arg_count) << DEFAULT;
        if (expected_arg_count == 1) {
            oss << " argument";
        } else {
            oss << " arguments";
        }
        oss << " for initializer but got " << YELLOW << std::to_string(actual_arg_count) << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expected " + std::to_string(expected_arg_count);
        if (expected_arg_count == 1) {
            d.message.append(" argument");
        } else {
            d.message.append(" arguments");
        }
        d.message.append(" for initializer but got ");
        d.message.append(std::to_string(actual_arg_count));
        return d;
    }

  private:
    size_t expected_arg_count;
    size_t actual_arg_count;
};
