#ifndef __ERR_LIT_UNTERMINATED_STRING_HPP__
#define __ERR_LIT_UNTERMINATED_STRING_HPP__

#include "error/error_types/base_error.hpp"

class ErrLitUnterminatedString : public BaseError {
  public:
    ErrLitUnterminatedString(const ErrorType error_type, const std::string &file, int line, int column) :
        BaseError(error_type, file, line, column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "The string literal was opened but never closed!";
        return oss.str();
    }
};

#endif
