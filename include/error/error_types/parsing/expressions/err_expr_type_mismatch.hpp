#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"
#include "types.hpp"

/// @class `ErrExprTypeMismatch`
/// @brief Represents type mismatch errors
class ErrExprTypeMismatch : public BaseError {
  public:
    ErrExprTypeMismatch(                       //
        const ErrorType error_type,            //
        const Hash &file_hash,                 //
        const token_slice &tokens,             //
        const std::shared_ptr<Type> &expected, //
        const std::shared_ptr<Type> &type      //
        ) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column),
        expected(expected),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Type mismatch of expression\n";
        oss << "    ├─ Expected: " << YELLOW << expected->to_string() << DEFAULT << "\n";
        oss << "    └─ But got:  " << YELLOW << type->to_string() << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expression type mismatch, expected '" + expected->to_string() + "' but got '" + type->to_string() + "'";
        return d;
    }

  private:
    /// @var `expected`
    /// @brief The expected type
    std::shared_ptr<Type> expected;

    /// @var `type`
    /// @brief The actual present type
    std::shared_ptr<Type> type;
};
