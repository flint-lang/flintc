#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprInitializerDuplicateField : public BaseError {
  public:
    ErrExprInitializerDuplicateField( //
        const ErrorType error_type,   //
        const Hash &file_hash,        //
        const unsigned int line,      //
        const unsigned int column,    //
        const std::string &field_name //
        ) :
        BaseError(error_type, file_hash, line, column, field_name.size()),
        field_name(field_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Initializer contains field '" << YELLOW << field_name << DEFAULT << "' twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Initializer contains field '" + field_name + "' twice";
        return d;
    }

  private:
    std::string field_name;
};
