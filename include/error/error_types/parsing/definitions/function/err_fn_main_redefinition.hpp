#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "types.hpp"

class ErrFnMainRedefinition : public BaseError {
  public:
    ErrFnMainRedefinition(             //
        const ErrorType error_type,    //
        const Hash &file_hash,         //
        const token_slice &tokens,     //
        const FunctionNode *first_main //
        ) :
        BaseError(error_type, file_hash, tokens),
        first_main(first_main) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Redefinition of the '" << YELLOW << "main" << DEFAULT << "' function\n";
        oss << "├─ First defined at " << GREEN << cwd_relative(first_main->file_hash.path) << ":" << first_main->line << ":"
            << first_main->column << DEFAULT << "\n";
        oss << "└─ Each project is only allowed to contain a single main function";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Redefinition of the main function";
        return d;
    }

  private:
    const FunctionNode *first_main;
};
