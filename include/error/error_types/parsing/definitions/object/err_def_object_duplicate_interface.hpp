#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefObjectDuplicateInterface : public BaseError {
  public:
    ErrDefObjectDuplicateInterface(  //
        const ErrorType error_type,  //
        const Hash &file_hash,       //
        const unsigned int line,     //
        const unsigned int column,   //
        const std::string &interface //
        ) :
        BaseError(error_type, file_hash, line, column, interface.size()),
        interface(interface) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Object implements the same interface '" << YELLOW << interface << DEFAULT << "' twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Object implements the same interface '" + interface + "' twice";
        return d;
    }

  private:
    std::string interface;
};
