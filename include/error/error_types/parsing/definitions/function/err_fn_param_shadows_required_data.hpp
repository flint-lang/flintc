#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnParamShadowsRequiredData : public BaseError {
  public:
    ErrFnParamShadowsRequiredData(  //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const token_slice &tokens   //
        ) :
        BaseError(error_type, file_hash, tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Function parameter shadows data required by func module";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Function parameter shadows data required by func module";
        return d;
    }
};
