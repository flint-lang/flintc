#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrLitCharLongerThanSingleCharacter : public BaseError {
  public:
    ErrLitCharLongerThanSingleCharacter( //
        const ErrorType error_type,      //
        const std::string &file,         //
        int line,                        //
        int column,                      //
        const std::string &text          //
        ) :
        BaseError(error_type, file, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Expected end of u8 literal " << CYAN << "'" << DEFAULT;
        oss << " but got '" << YELLOW << text << DEFAULT << "'";
        return oss.str();
    }

  private:
    std::string text;
};
