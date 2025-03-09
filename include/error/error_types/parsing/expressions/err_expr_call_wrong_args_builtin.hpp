#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "lexer/builtins.hpp"
#include "types.hpp"

class ErrExprCallWrongArgsBuiltin : public BaseError {
  public:
    ErrExprCallWrongArgsBuiltin(                  //
        const ErrorType error_type,               //
        const std::string &file,                  //
        const token_list &tokens,                 //
        const std::string &function_name,         //
        const std::vector<std::string> &arg_types //
        ) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens),
        function_name(function_name),
        arg_types(arg_types) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Could not parse function call: '" << YELLOW << get_token_string(tokens, {}) << DEFAULT
            << "'\n -- No variant of the builtin '" << YELLOW << get_function_signature_string(function_name, arg_types) << DEFAULT
            << "' function available.\n -- Available signatures are:";
        for (const auto &[param_types, return_types] : builtin_function_types.at(builtin_functions.at(function_name))) {
            const std::vector<std::string> param_types_str(param_types.begin(), param_types.end());
            oss << "\n    " << GREEN << get_function_signature_string(function_name, param_types_str) << DEFAULT;
        }
        return oss.str();
    }

  private:
    token_list tokens;
    std::string function_name;
    std::vector<std::string> arg_types;
};
