#ifndef __ERR_LEXING_HPP__
#define __ERR_LEXING_HPP__

#include "base_error.hpp"

class LexingError : public BaseError {
  public:
    LexingError(std::string &text, std::string &file, int line, int column) :
        BaseError("Lexing Error", file, line, column),
        text(text) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "\nOffending text: " << text;
        return oss.str();
    }

  private:
    std::string text;
};

#endif
