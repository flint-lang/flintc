#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrLitIntTooSmall : public BaseError {
  public:
    ErrLitIntTooSmall(                //
        const ErrorType error_type,   //
        const Hash &file_hash,        //
        const PosTriple &pos,         //
        const size_t N,               //
        const std::string &min_value, //
        const bool is_signed          //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        N(N),
        min_value(min_value),
        is_signed(is_signed) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Integer literal is too small to fit into target type '";
        oss << YELLOW << (is_signed ? 'i' : 'u') << std::to_string(N) << DEFAULT << "'\n";
        oss << "└─ The smallest allowed value is '" << BLUE << min_value << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Integer literal too small to fit into type, smallest allowed value is: " + min_value;
        return d;
    }

  private:
    size_t N;
    std::string min_value;
    bool is_signed;
};
