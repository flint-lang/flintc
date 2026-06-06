#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"

#include <algorithm>

class ErrDefEntityFnRedefinition : public BaseError {
  public:
    ErrDefEntityFnRedefinition(       //
        const ErrorType error_type,   //
        const Hash &file_file,        //
        const FunctionNode *function, //
        const FunctionNode *original  //
        ) :
        BaseError(error_type, file_file, function->line, function->column, function->length),
        function(function),
        original(original) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Redefinition of function: " << YELLOW << original->name << DEFAULT
            << " from func module defined at " << GREEN << GREEN << cwd_relative(original->file_hash.path) << ":" << original->line << ":"
            << original->column << DEFAULT << "\n";
        oss << "├─ The explicit signatures of the function from the func-module matches the newly defined entity function and creates a collision\n";

        const auto &original_dot_idx = std::find(original->name.begin(), original->name.end(), '.');
        assert(original_dot_idx != original->name.end());
        const size_t original_dot_pos = std::distance(original->name.begin(), original_dot_idx);
        const std::string original_ref = original->name.substr(0, original_dot_pos) + "::" + original->name.substr(original_dot_pos + 1);

        const auto &function_dot_idx = std::find(function->name.begin(), function->name.end(), '.');
        assert(function_dot_idx != function->name.end());
        const size_t function_dot_pos = std::distance(function->name.begin(), function_dot_idx);
        const std::string function_ref = function->name.substr(0, function_dot_pos) + "::" + function->name.substr(function_dot_pos + 1);

        oss << "└─ Have you considered linking? " << BLUE << original_ref << " -> " << function_ref << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Redefinition of function '" + function->name + "' from func module";
        return d;
    }

  private:
    const FunctionNode *function;
    const FunctionNode *original;
};
