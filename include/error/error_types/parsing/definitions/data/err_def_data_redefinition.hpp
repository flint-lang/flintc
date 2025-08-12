#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefDataRedefinition : public BaseError {
  public:
    ErrDefDataRedefinition(          //
        const ErrorType error_type,  //
        const std::string &file,     //
        const unsigned int line,     //
        const unsigned int column,   //
        const std::string &data_name //
        ) :
        BaseError(error_type, file, line, column, data_name.size()),
        data_name(data_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Redefinition of data module: " << YELLOW << data_name << DEFAULT;
        return oss.str();
    }

  private:
    const std::string data_name;
};
