#ifndef __ERR_LEXING_HPP__
#define __ERR_LEXING_HPP__

#include "error/error_types/base_error.hpp"

class ErrLexing : public BaseError {
  public:
    ErrLexing(const std::string &message, const std::string &file, int line, int column, const std::string &text) :
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
