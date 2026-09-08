#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprArrayNeedsInitializer : public BaseError {
  public:
    ErrExprArrayNeedsInitializer(         //
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
        oss << BaseError::to_string() << "└─ Array initializer needs to be provided because type '" << YELLOW << type->to_string()
            << DEFAULT << "' is not default-constructible";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Array initializer needs to be provided because type '" + type->to_string() + "' is not default-constructible";
        return d;
    }

  private:
    std::shared_ptr<Type> type;
};
