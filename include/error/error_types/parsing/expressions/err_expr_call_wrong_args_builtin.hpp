#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "lexer/builtins.hpp"
#include "parser/ast/definitions/import_node.hpp"
#include "types.hpp"

#include <algorithm>

class ErrExprCallWrongArgsBuiltin : public BaseError {
  public:
    ErrExprCallWrongArgsBuiltin(                                                        //
        const ErrorType error_type,                                                     //
        const std::string &file,                                                        //
        const token_slice &tokens,                                                      //
        const std::string &function_name,                                               //
        const std::vector<std::shared_ptr<Type>> &arg_types,                            //
        const std::unordered_map<std::string, ImportNode *const> &imported_core_modules //
        ) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column),
        tokens(tokens),
        function_name(function_name),
        arg_types(arg_types),
        imported_core_modules(imported_core_modules) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Could not parse function call: '" << YELLOW << get_token_string(tokens, {}) << DEFAULT
            << "'\n -- No variant of the builtin '" << YELLOW << get_function_signature_string(function_name, arg_types) << DEFAULT
            << "' function available.\n";
        std::optional<overloads> function_overloads;
        for (const auto &[module_name, import_node] : imported_core_modules) {
            const auto &module_functions = core_module_functions.at(module_name);
            if (module_functions.find(function_name) != module_functions.end()) {
                function_overloads = module_functions.at(function_name);
            }
        }
        if (!function_overloads.has_value()) {
            return oss.str();
        }
        oss << " -- Available signatures are:";
        for (const auto &[param_types_str, return_types_str] : function_overloads.value()) {
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
    std::unordered_map<std::string, ImportNode *const> imported_core_modules;
};
