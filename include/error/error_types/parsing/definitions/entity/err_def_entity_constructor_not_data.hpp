#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefEntityConstructorNotData : public BaseError {
  public:
    ErrDefEntityConstructorNotData(       //
        const ErrorType error_type,       //
        const Hash &file_hash,            //
        const unsigned int line,          //
        const unsigned int column,        //
        const std::shared_ptr<Type> &type //
        ) :
        BaseError(error_type, file_hash, line, column, type->to_string().size()) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Entity constructor is only allowed to contain data typed values";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Entity constructor is only allowed to contain data typed values";
        return d;
    }
};
