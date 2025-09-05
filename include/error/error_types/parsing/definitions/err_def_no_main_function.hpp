#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefNoMainFunction : public BaseError {
  public:
    ErrDefNoMainFunction(const ErrorType error_type, const std::string &file) :
        BaseError(error_type, file, 1, 1) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ No main function found in the project";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "No main function found in the project";
        return d;
    }
};
