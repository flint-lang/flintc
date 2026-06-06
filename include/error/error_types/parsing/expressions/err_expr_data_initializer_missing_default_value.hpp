#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprDataInitializerMissingDefaultValue : public BaseError {
  public:
    ErrExprDataInitializerMissingDefaultValue( //
        const ErrorType error_type,            //
        const Hash &file_hash,                 //
        const token_slice &tokens,             //
        const std::string field_name           //
        ) :
        BaseError(error_type, file_hash, tokens),
        field_name(field_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Field '" << YELLOW << field_name << DEFAULT << "' cannot be default-initialized\n";
        oss << "└─ If you want to default-initialize '" << YELLOW << field_name << DEFAULT
            << "' then you need to edit the field declaration in the data type";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Field '" + field_name + "' cannot be default-initialized";
        return d;
    }

  private:
    std::string field_name;
};
