#pragma once

#include "error/error_types/base_error.hpp"

class ErrPtrNotAllowedInInternalFunctionDefinition : public BaseError {
  public:
    ErrPtrNotAllowedInInternalFunctionDefinition( //
        const ErrorType error_type,               //
        const std::shared_ptr<Type> &type,        //
        const std::string &file_name,             //
        const unsigned int line,                  //
        const unsigned int column                 //
        ) :
        BaseError(error_type, file_name, line, column, type->to_string().length()) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Pointer types are not allowed in non-extern functions\n";
        oss << "└─ A pointer type 'T*' can only be used when defining or calling 'extern' functions";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Pointer types are not allowed in non-extern functions";
        return d;
    }
};
