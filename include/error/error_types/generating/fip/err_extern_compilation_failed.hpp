#pragma once

#include "error/error_types/base_error.hpp"

class ErrExternCompilationFailed : public BaseError {
  public:
    ErrExternCompilationFailed(const ErrorType error_type) :
        BaseError(error_type, "", 0, 0, 0) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Compilation of external dependency failed\n";
        oss << "└─ This is not a problem of Flint itself but of one of your external's code (like C code)";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Compilation of external dependency failed";
        return d;
    }
};
