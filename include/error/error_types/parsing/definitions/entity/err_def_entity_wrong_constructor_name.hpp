#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefEntityWrongConstructorName : public BaseError {
  public:
    ErrDefEntityWrongConstructorName(     //
        const ErrorType error_type,       //
        const std::string &file,          //
        const unsigned int line,          //
        const unsigned int column,        //
        const std::string &expected_name, //
        const std::string &actual_name    //
        ) :
        BaseError(error_type, file, line, column),
        expected_name(expected_name),
        actual_name(actual_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Expected entity constructor name '" << YELLOW << expected_name << DEFAULT << "' but got '"
            << YELLOW << actual_name << DEFAULT << "'";
        return oss.str();
    }

  private:
    std::string expected_name;
    std::string actual_name;
};
