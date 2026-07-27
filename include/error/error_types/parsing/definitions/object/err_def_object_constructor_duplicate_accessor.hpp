#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefObjectConstructorDuplicateAccessor : public BaseError {
  public:
    ErrDefObjectConstructorDuplicateAccessor( //
        const ErrorType error_type,           //
        const Hash &file_hash,                //
        const unsigned int line,              //
        const unsigned int column,            //
        const std::string &accessor           //
        ) :
        BaseError(error_type, file_hash, line, column, accessor.size()),
        accessor(accessor) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Object constructor contains accessor '" << YELLOW << accessor << DEFAULT << "' twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Object constructor contains accessor '" + accessor + "' twice";
        return d;
    }

  private:
    std::string accessor;
};
