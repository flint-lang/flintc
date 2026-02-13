#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrAmbiguousModuleTag : public BaseError {
  public:
    ErrAmbiguousModuleTag(             //
        const ErrorType error_type,    //
        const Hash &file_hash,         //
        const unsigned int line,       //
        const unsigned int column,     //
        const unsigned int length,     //
        const std::string &module_name //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        module_name(module_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Extern module tag 'Fip." << YELLOW << module_name << DEFAULT << "' is ambiguous\n";
        oss << "└─ Check your configs in '" << CYAN << ".fip/config/" << DEFAULT << "' to see which interop modules provide it";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Extern module tag '" + module_name + "' is ambiguous";
        return d;
    }

  private:
    std::string module_name;
};
