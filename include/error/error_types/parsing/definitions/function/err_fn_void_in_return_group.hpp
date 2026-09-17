#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnVoidInReturnGroup : public BaseError {
  public:
    ErrFnVoidInReturnGroup(         //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const PosTriple &pos        //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Functions cannot return a group containing a void type";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Functions cannot return a group containing a void type";
        return d;
    }
};
