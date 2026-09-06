#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprInitializerFieldMixedStyles : public BaseError {
  public:
    ErrExprInitializerFieldMixedStyles( //
        const ErrorType error_type,     //
        const Hash &file_hash,          //
        const PosTriple &pos,           //
        const bool fields_have_names    //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        fields_have_names(fields_have_names) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Use of mixed field styles is not allowed\n";
        if (fields_have_names) {
            oss << "└─ Expected all fields to be named: " << BLUE << ".value = expression" << DEFAULT;
        } else {
            oss << "└─ Expected all fields to be positional expressions";
        }
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Use of mixed field styles is not allowed";
        return d;
    }

  private:
    bool fields_have_names;
};
