#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefDataDuplicateFieldName : public BaseError {
  public:
    ErrDefDataDuplicateFieldName(          //
        const ErrorType error_type,        //
        const std::string &file,           //
        const unsigned int line,           //
        const unsigned int column,         //
        const std::string &duplicate_field //
        ) :
        BaseError(error_type, file, line, column, duplicate_field.size()),
        duplicate_field(duplicate_field) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Duplicate data field '" << YELLOW << duplicate_field << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Duplicate data field '" + duplicate_field + "'";
        return d;
    }

  private:
    std::string duplicate_field;
};
