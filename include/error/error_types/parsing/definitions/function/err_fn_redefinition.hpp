#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"

class ErrFnRedefinition : public BaseError {
  public:
    ErrFnRedefinition(                //
        const ErrorType error_type,   //
        const Hash &file_file,        //
        const FunctionNode *function, //
        const FunctionNode *original  //
        ) :
        BaseError(error_type, file_file, function->line, function->column, function->length),
        original(original),
        function_string(function->get_signature_string(0, false, false, false, false)) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Redefinition of function: " << YELLOW << function_string << DEFAULT << "\n";
        oss << "└─ First defined at: " << GREEN << cwd_relative(original->file_hash, original->line, original->column) << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Redefinition of function '" + function_string + "'";
        return d;
    }

  private:
    const FunctionNode *original;
    std::string function_string;
};
