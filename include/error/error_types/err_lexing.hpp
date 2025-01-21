#ifndef __ERR_LEXING_HPP__
#define __ERR_LEXING_HPP__

#include "base_error.hpp"

class ErrLexing : public BaseError {
  public:
    ErrLexing(const std::string &message, std::string &file, int line, int column, std::string &text) :
        BaseError(file, line, column, message),
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
