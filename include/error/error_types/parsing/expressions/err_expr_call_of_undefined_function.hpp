#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrExprCallOfUndefinedFunction : public BaseError {
  public:
    ErrExprCallOfUndefinedFunction(                         //
        const ErrorType error_type,                         //
        const Hash &file_hash,                              //
        const token_slice &tokens,                          //
        const std::string &function_name,                   //
        const std::vector<std::shared_ptr<Type>> &arg_types //
        ) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, (tokens.first + 1)->column - tokens.first->column),
        function_name(function_name),
        arg_types(arg_types) {}

    [[nodiscard]]
    std::string to_string() const override;

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Call of undefined function '" + function_name + "'";
        return d;
    }

  private:
    std::string function_name;
    std::vector<std::shared_ptr<Type>> arg_types;
};
