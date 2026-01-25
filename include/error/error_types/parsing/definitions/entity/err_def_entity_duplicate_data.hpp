#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefEntityDuplicateData : public BaseError {
  public:
    ErrDefEntityDuplicateData(       //
        const ErrorType error_type,  //
        const Hash &file_hash,       //
        const unsigned int line,     //
        const unsigned int column,   //
        const std::string &data_type //
        ) :
        BaseError(error_type, file_hash, line, column, data_type.size()),
        data_type(data_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Entity type defines data module '" << YELLOW << data_type << DEFAULT << "' twice";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Entity type defines data module '" + data_type + "' twice";
        return d;
    }

  private:
    std::string data_type;
};
