#ifndef __ERR_LIT_CHAR_LONGER_THAN_SINGLE_CHARACTER_HPP__
#define __ERR_LIT_CHAR_LONGER_THAN_SINGLE_CHARACTER_HPP__

#include "error/error_types/base_error.hpp"

class ErrLitCharLongerThanSingleCharacter : public BaseError {
  public:
    ErrLitCharLongerThanSingleCharacter(const ErrorType error_type, const std::string &file, int line, int column,
        const std::string &text) :
        BaseError(error_type, file, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Expected end of char ' but got '" << text << "'";
        return oss.str();
    }

  private:
    std::string text;
};

#endif
