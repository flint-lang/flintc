#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"

class ErrExprFieldAccessOnEntity : public BaseError {
  public:
    ErrExprFieldAccessOnEntity(     //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file_hash, tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Direct field accesses are not allowed on entities";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Direct field accesses are not allowed on entities";
        return d;
    }
};
