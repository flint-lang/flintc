#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/import_node.hpp"

class ErrImportNonexistentFile : public BaseError {
  public:
    ErrImportNonexistentFile(const ErrorType error_type, const Hash &file_hash, const ImportNode *import) :
        BaseError(error_type, file_hash, import->line, import->column, import->length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Imported file does not exist";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Imported file does not exist";
        return d;
    }
};
