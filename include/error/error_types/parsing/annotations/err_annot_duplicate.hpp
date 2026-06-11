#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"

class ErrAnnotDuplicate : public BaseError {
  public:
    ErrAnnotDuplicate(                //
        const ErrorType error_type,   //
        const Hash &file_hash,        //
        const token_slice &tokens,    //
        const std::string &annotation //
        ) :
        BaseError(error_type, file_hash, tokens),
        annotation(annotation) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Defined annotation '" << YELLOW << annotation << DEFAULT << "' twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Defined annotation '" + annotation + "' twice";
        return d;
    }

  private:
    std::string annotation;
};
