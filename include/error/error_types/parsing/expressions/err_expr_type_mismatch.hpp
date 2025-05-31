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
        const std::string &file,               //
        const token_slice &tokens,             //
        const std::shared_ptr<Type> &expected, //
        const std::shared_ptr<Type> &type      //
        ) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column),
        tokens(tokens),
        expected(expected),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Type mismatch of expression " << YELLOW << get_token_string(tokens, {}) << DEFAULT
            << "\n -- Expected " << YELLOW << expected->to_string() << DEFAULT << " but got " << YELLOW << type->to_string() << DEFAULT;
        return oss.str();
    }

  private:
    /// @var `tokens`
    /// @brief The tokens whose resulting type was wrong
    token_slice tokens;

    /// @var `expected`
    /// @brief The expected type
    std::shared_ptr<Type> expected;

    /// @var `type`
    /// @brief The actual present type
    std::shared_ptr<Type> type;
};
