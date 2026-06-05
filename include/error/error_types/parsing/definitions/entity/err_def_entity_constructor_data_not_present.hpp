#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefEntityConstructorDataNotPresent : public BaseError {
  public:
    ErrDefEntityConstructorDataNotPresent( //
        const ErrorType error_type,        //
        const Hash &file_hash,             //
        const unsigned int line,           //
        const unsigned int column,         //
        const std::shared_ptr<Type> &type  //
        ) :
        BaseError(error_type, file_hash, line, column, type->to_string().size()) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ This data type is not part of the entity\n";
        oss << "└─ Only data from extended entities or defined in the 'data:' section can be used in the constructor";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Data type not part of entity";
        return d;
    }
};
