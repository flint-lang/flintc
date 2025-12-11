#pragma once

#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrImportExitedCWD : public BaseError {
  public:
    ErrImportExitedCWD(const ErrorType error_type, const Hash &file_hash, const token_slice &tokens) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ The import tried to escape the current working directory of the compiler\n";
        oss << "├─ If you really need to reference a file outside this working directory, move up a directory\n";
        oss << "├─ Only files within the current working directory are allowed to be accessed by the compiler\n";
        oss << "└─ See " << get_wiki_link()
            << "/beginners_guide/7_imports/5_relative_paths.html#exiting-the-cwd-is-considered-an-error for more information";

        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Use statement not at top level";
        return d;
    }
};
