#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"

class ErrExprCallOnWrongInstanceType : public BaseError {
  public:
    ErrExprCallOnWrongInstanceType( //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file_hash, tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Instance calls are only allowed on variables of type 'entity' or 'func'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Instance calls are only allowed on variables of type 'entity' or 'func'";
        return d;
    }
};
