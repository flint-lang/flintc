#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"

class ErrExprCastInvalid : public BaseError {
  public:
    ErrExprCastInvalid(                      //
        const ErrorType error_type,          //
        const Hash &file_hash,               //
        const unsigned int line,             //
        const unsigned int column,           //
        const unsigned int length,           //
        const std::shared_ptr<Type> &target, //
        const std::shared_ptr<Type> &type    //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        target(target),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Cannot cast value\n";
        oss << "    ├─ From " << YELLOW << type->to_string() << DEFAULT << "\n";
        oss << "    └─ To   " << YELLOW << target->to_string() << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Invalid cast, '" + type->to_string() + "' not castable to '" + target->to_string() + "'";
        return d;
    }

  private:
    std::shared_ptr<Type> target;
    std::shared_ptr<Type> type;
};
