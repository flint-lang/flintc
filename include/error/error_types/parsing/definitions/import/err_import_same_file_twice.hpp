#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/import_node.hpp"

class ErrImportSameFileTwice : public BaseError {
  public:
    ErrImportSameFileTwice(const ErrorType error_type, const Hash &file_hash, const ImportNode *import) :
        BaseError(error_type, file_hash, import->line, import->column, import->length),
        is_core_module(!std::holds_alternative<Hash>(import->path)) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Imported the same " << (is_core_module ? "Core module" : "file") << " twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        const std::string inserter = is_core_module ? "Core module" : "file";
        d.message = "Imported the same " + inserter + " twice";
        return d;
    }

  private:
    const bool is_core_module;
};
