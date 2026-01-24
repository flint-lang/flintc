#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefEntityMissingData : public BaseError {
  public:
    ErrDefEntityMissingData(          //
        const ErrorType error_type,   //
        const Hash &file_hash,        //
        const unsigned int line,      //
        const unsigned int column,    //
        const unsigned int length,    //
        const std::string &data_type, //
        const std::string &func_type  //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        data_type(data_type),
        func_type(func_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Entity type is missing data '" << YELLOW << data_type << DEFAULT
            << "' required by func module '" << YELLOW << func_type << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Entity type is missing data '" + data_type + "' required by func module '" + func_type + "'";
        return d;
    }

  private:
    std::string data_type;
    std::string func_type;
};
