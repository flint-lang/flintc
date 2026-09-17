#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnVoidParamType : public BaseError {
  public:
    ErrFnVoidParamType(             //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const PosTriple &pos        //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Function parameters cannot be of type void";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Function parameters cannot be of type void";
        return d;
    }
};
