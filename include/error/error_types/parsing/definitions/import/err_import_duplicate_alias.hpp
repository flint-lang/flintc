#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/import_node.hpp"

class ErrImportDuplicateAlias : public BaseError {
  public:
    ErrImportDuplicateAlias(           //
        const ErrorType error_type,    //
        const Hash &file_hash,         //
        const unsigned int line,       //
        const unsigned int column,     //
        const unsigned int length,     //
        const std::string &alias,      //
        const ImportNode *first_import //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        alias(alias),
        first_import(first_import) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ The import alias '" << YELLOW << alias << DEFAULT << "' is already taken\n";
        oss << "└─ First used at: " << GREEN << cwd_relative(first_import->file_hash.path) << ":" << first_import->line << ":"
            << first_import->column << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The import alias '" + alias + "' is already taken";
        return d;
    }

  private:
    std::string alias;
    const ImportNode *first_import;
};
