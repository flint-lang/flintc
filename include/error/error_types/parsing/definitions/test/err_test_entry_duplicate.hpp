#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"

class ErrTestEntryDuplicate : public BaseError {
  public:
    ErrTestEntryDuplicate(                //
        const ErrorType error_type,       //
        const Hash &file_hash,            //
        const unsigned int line,          //
        const unsigned int column,        //
        const unsigned int length,        //
        const FunctionNode *first_entry   //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        first_entry(first_entry) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ The '" << YELLOW << "#test_entry" << DEFAULT << "' function is already defined in this program\n";
        oss << "├─ First defined at " << GREEN << cwd_relative(first_entry->file_hash, first_entry->line, first_entry->column) << DEFAULT
            << "\n";
        oss << "└─ Only one '#test_entry' function is allowed in the entire codebase";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The '#test_entry' function is already defined in this program";
        return d;
    }

  private:
    const FunctionNode *first_entry;
};