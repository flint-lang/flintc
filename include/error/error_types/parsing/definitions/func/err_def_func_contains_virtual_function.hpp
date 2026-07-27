#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefFuncContansVirtualFunction : public BaseError {
  public:
    ErrDefFuncContansVirtualFunction( //
        const ErrorType error_type,   //
        const Hash &file_file,        //
        const token_slice &tokens     //
        ) :
        BaseError(error_type, file_file, tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Func modules are not allowed to contain virtual functions\n";
        oss << "└─ If you want polymorphic behaviour, use the 'interface' type instead";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Func modules are not allowed to contain virtual functions";
        return d;
    }
};
