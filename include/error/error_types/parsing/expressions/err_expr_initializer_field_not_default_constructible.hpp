#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprInitializerFieldNotDefaultConstructible : public BaseError {
  public:
    ErrExprInitializerFieldNotDefaultConstructible( //
        const ErrorType error_type,                 //
        const Hash &file_hash,                      //
        const PosTriple &pos,                       //
        const std::string &field_name,              //
        const std::shared_ptr<Type> &field_type     //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        field_name(field_name),
        field_type(field_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Field '" << YELLOW << field_name << DEFAULT << "' of type '" << YELLOW
            << field_type->to_string() << DEFAULT << "' is not default-constructible";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Field '" + field_name + "' of type '" + field_type->to_string() + "' is not default-constructible";
        return d;
    }

  private:
    std::string field_name;
    std::shared_ptr<Type> field_type;
};
