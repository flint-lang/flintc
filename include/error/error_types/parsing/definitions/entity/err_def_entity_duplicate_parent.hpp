#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefEntityDuplicateParent : public BaseError {
  public:
    ErrDefEntityDuplicateParent(       //
        const ErrorType error_type,    //
        const Hash &file_hash,         //
        const unsigned int line,       //
        const unsigned int column,     //
        const std::string &parent_type //
        ) :
        BaseError(error_type, file_hash, line, column, parent_type.size()),
        parent_accessor(parent_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Entity extends the same parent '" << YELLOW << parent_accessor << DEFAULT << "' twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Entity extends the same parent '" + parent_accessor + "' twice";
        return d;
    }

  private:
    std::string parent_accessor;
};
