#ifndef __ERR_UNEXPECTED_TOKEN_PIPE_HPP__
#define __ERR_UNEXPECTED_TOKEN_PIPE_HPP__

#include "error/error_types/base_error.hpp"

class ErrUnexpectedTokenPipe : public BaseError {
  public:
    ErrUnexpectedTokenPipe(const ErrorType error_type, const std::string &file, int line, int column, const char &text) :
        BaseError(error_type, file, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Unknown character '" << text << "' when expecting '>' for the pipe";
        return oss.str();
    }

  private:
    char text;
};

#endif
