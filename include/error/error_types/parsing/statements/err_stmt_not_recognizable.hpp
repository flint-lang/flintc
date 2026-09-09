#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"

class ErrStmtNotRecognizable : public BaseError {
  public:
    ErrStmtNotRecognizable(         //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const PosTriple &pos        //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Statement not recognizable by matcher";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Statement not recognizable by matcher";
        return d;
    }
};
