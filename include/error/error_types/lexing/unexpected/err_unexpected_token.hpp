#ifndef __ERR_UNEXPECTED_TOKEN_HPP__
#define __ERR_UNEXPECTED_TOKEN_HPP__

#include "error/error_types/base_error.hpp"

class ErrUnexpectedToken : public BaseError {
  public:
    ErrUnexpectedToken(const ErrorType error_type, const std::string &file, int line, int column, const char &text) :
        BaseError(error_type, file, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Unknown character '" << text << "'";
        return oss.str();
    }

  private:
    char text;
};

#endif
