#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrExprCallOfUndefinedFunction : public BaseError {
  public:
    ErrExprCallOfUndefinedFunction(                         //
        const ErrorType error_type,                         //
        const std::string &file,                            //
        const token_slice &tokens,                          //
        const std::string &function_name,                   //
        const std::vector<std::shared_ptr<Type>> &arg_types //
        ) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, (tokens.first + 1)->column - tokens.first->column),
        file_name(file),
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
    std::string file_name;
    std::string function_name;
    std::vector<std::shared_ptr<Type>> arg_types;
};
