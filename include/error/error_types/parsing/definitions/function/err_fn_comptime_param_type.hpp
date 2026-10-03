#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnComptimeParamType : public BaseError {
  public:
    ErrFnComptimeParamType(               //
        const ErrorType error_type,       //
        const Hash &file_hash,            //
        const PosTriple &pos,             //
        const std::shared_ptr<Type> &type //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Parameters of runtime function cannot be of compile-time only type '" << YELLOW
            << type->to_string() << DEFAULT << "'\n";
        oss << "└─ If you want a function to operate on compile-time types, you need to define the function as compile-time only using '"
            << BLUE << "@def" << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Parameters of runtime function cannot be of a compile-time only type";
        return d;
    }

  private:
    std::shared_ptr<Type> type;
};
