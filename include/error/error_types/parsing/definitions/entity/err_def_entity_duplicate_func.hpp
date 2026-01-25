#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefEntityDuplicateFunc : public BaseError {
  public:
    ErrDefEntityDuplicateFunc(       //
        const ErrorType error_type,  //
        const Hash &file_hash,       //
        const unsigned int line,     //
        const unsigned int column,   //
        const std::string &func_type //
        ) :
        BaseError(error_type, file_hash, line, column, func_type.size()),
        func_type(func_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Entity type defines func module '" << YELLOW << func_type << DEFAULT << "' twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Entity type defines func module '" + func_type + "' twice";
        return d;
    }

  private:
    std::string func_type;
};
