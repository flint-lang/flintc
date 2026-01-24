#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"

class ErrDefFunctionRedefinition : public BaseError {
  public:
    ErrDefFunctionRedefinition(       //
        const ErrorType error_type,   //
        const Hash &file_file,        //
        const FunctionNode *function, //
        const FunctionNode *original  //
        ) :
        BaseError(error_type, file_file, function->line, function->column, function->length),
        original(original) {
        function_string = function->name + "(";
        for (size_t i = 0; i < function->parameters.size(); i++) {
            if (i > 0) {
                function_string += ", ";
            }
            function_string += std::get<0>(function->parameters.at(i))->to_string();
        }
        function_string += ")";
    }

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Redefinition of function: " << YELLOW << function_string << DEFAULT << "\n";
        oss << "└─ First defined at: " << GREEN << original->file_hash.to_string() << ":" << original->line << ":" << original->column
            << DEFAULT;
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
