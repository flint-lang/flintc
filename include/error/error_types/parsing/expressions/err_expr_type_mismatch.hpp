#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/type/type.hpp"
#include "types.hpp"

#include <variant>

/// @class `ErrExprTypeMismatch`
/// @brief Represents type mismatch errors
class ErrExprTypeMismatch : public BaseError {
  public:
    ErrExprTypeMismatch(                                                                         //
        const ErrorType error_type,                                                              //
        const std::string &file,                                                                 //
        const token_list &tokens,                                                                //
        const std::variant<std::shared_ptr<Type>, std::vector<std::shared_ptr<Type>>> &expected, //
        const std::variant<std::shared_ptr<Type>, std::vector<std::shared_ptr<Type>>> &type      //
        ) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens),
        expected(expected),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Type mismatch of expression " << YELLOW << get_token_string(tokens, {}) << DEFAULT
            << "\n -- Expected " << YELLOW << get_type_string(expected) << DEFAULT << " but got " << YELLOW << get_type_string(type)
            << DEFAULT;
        return oss.str();
    }

  private:
    /// @var `tokens`
    /// @brief The tokens whose resulting type was wrong
    token_list tokens;

    /// @var `expected`
    /// @brief The expected type
    std::variant<std::shared_ptr<Type>, std::vector<std::shared_ptr<Type>>> expected;

    /// @var `type`
    /// @brief The actual present type
    std::variant<std::shared_ptr<Type>, std::vector<std::shared_ptr<Type>>> type;
};
