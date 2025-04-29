#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "lexer/builtins.hpp"
#include "types.hpp"
#include <algorithm>

class ErrExprCallWrongArgsBuiltin : public BaseError {
  public:
    ErrExprCallWrongArgsBuiltin(                            //
        const ErrorType error_type,                         //
        const std::string &file,                            //
        const token_slice &tokens,                          //
        const std::string &function_name,                   //
        const std::vector<std::shared_ptr<Type>> &arg_types //
        ) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column),
        tokens(tokens),
        function_name(function_name),
        arg_types(arg_types) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Could not parse function call: '" << YELLOW << get_token_string(tokens, {}) << DEFAULT
            << "'\n -- No variant of the builtin '" << YELLOW << get_function_signature_string(function_name, arg_types) << DEFAULT
            << "' function available.\n -- Available signatures are:";
        for (const auto &[param_types_str, return_types_str] : builtin_function_types.at(builtin_functions.at(function_name))) {
            std::vector<std::shared_ptr<Type>> param_types;
            std::transform(param_types_str.begin(), param_types_str.end(), param_types.begin(), Type::str_to_type);
            oss << "\n    " << GREEN << get_function_signature_string(function_name, param_types) << DEFAULT;
        }
        return oss.str();
    }

  private:
    token_slice tokens;
    std::string function_name;
    std::vector<std::shared_ptr<Type>> arg_types;
};
