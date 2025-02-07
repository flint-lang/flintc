#ifndef __ERR_UNEXPECTED_TOKEN_NUMBER_HPP__
#define __ERR_UNEXPECTED_TOKEN_NUMBER_HPP__

#include "error/error_types/base_error.hpp"

class ErrUnexpectedTokenNumber : public BaseError {
  public:
    ErrUnexpectedTokenNumber(const ErrorType error_type, const std::string &file, int line, int column, const char &text) :
        BaseError(error_type, file, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Expected number after '.' but got '" << text
            << "'.\n -- Floating point numbers have the form '5.3' and '5.' or '5.x' is not allowed";
        return oss.str();
    }

  private:
    char text;
};

#endif
