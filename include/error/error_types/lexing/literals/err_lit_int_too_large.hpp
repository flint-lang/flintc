#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrLitIntTooLarge : public BaseError {
  public:
    ErrLitIntTooLarge(                //
        const ErrorType error_type,   //
        const Hash &file_hash,        //
        const PosTriple &pos,         //
        const size_t N,               //
        const std::string &max_value, //
        const bool is_signed          //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        N(N),
        max_value(max_value),
        is_signed(is_signed) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Integer literal is too large to fit into target type '";
        oss << YELLOW << (is_signed ? 'i' : 'u') << std::to_string(N) << DEFAULT << "'\n";
        oss << "└─ The largest allowed value is '" << BLUE << max_value << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Integer literal too large to fit into type, largest allowed value is: " + max_value;
        return d;
    }

  private:
    size_t N;
    std::string max_value;
    bool is_signed;
};
