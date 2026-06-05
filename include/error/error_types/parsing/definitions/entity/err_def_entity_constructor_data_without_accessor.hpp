#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefEntityConstructorDataWitoutAccessor : public BaseError {
  public:
    ErrDefEntityConstructorDataWitoutAccessor( //
        const ErrorType error_type,            //
        const Hash &file_hash,                 //
        const unsigned int line,               //
        const unsigned int column,             //
        const std::shared_ptr<Type> &type,     //
        const std::string &accessor            //
        ) :
        BaseError(error_type, file_hash, line, column, type->to_string().size()),
        accessor(accessor) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ This data type was included in the entity using the data accessor '" << YELLOW << accessor
            << DEFAULT << "'\n";
        oss << "└─ Use the data accessor directly in the entity constructor";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Use data accessor '" + accessor + "' in entity constructor directly";
        return d;
    }

  private:
    std::string accessor;
};
