#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefEntityConstructorDuplicateData : public BaseError {
  public:
    ErrDefEntityConstructorDuplicateData( //
        const ErrorType error_type,       //
        const Hash &file_hash,            //
        const unsigned int line,          //
        const unsigned int column,        //
        const std::string &type_str       //
        ) :
        BaseError(error_type, file_hash, line, column, type_str.size()),
        type_str(type_str) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Entity constructor contains data type '" << YELLOW << type_str << DEFAULT << "' twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Entity constructor contains data type '" + type_str + "' twice";
        return d;
    }

  private:
    std::string type_str;
};
