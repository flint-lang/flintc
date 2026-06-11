#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"

class ErrExprArrayAccessNotAllowedOnType : public BaseError {
  public:
    ErrExprArrayAccessNotAllowedOnType(   //
        const ErrorType error_type,       //
        const Hash &file_hash,            //
        const token_slice &tokens,        //
        const std::shared_ptr<Type> &type //
        ) :
        BaseError(error_type, file_hash, tokens),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Array accesses are not allowed on type '" << YELLOW << type->to_string() << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Array accesses are not allowed on type '" + type->to_string() + "'";
        return d;
    }

  private:
    std::shared_ptr<Type> type;
};
