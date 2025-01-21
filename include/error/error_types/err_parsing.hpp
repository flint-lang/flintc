#ifndef __ERR_PARSING_HPP__
#define __ERR_PARSING_HPP__

#include "base_error.hpp"
#include "types.hpp"

class ErrParsing : public BaseError {
  public:
    ErrParsing(const std::string &message, const std::string &file, int line, int column, const token_list &tokens) :
        BaseError(file, line, column, message),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "\nOffending tokens: ";
        for (const auto &token : tokens) {
            oss << token.lexme << " ";
        }
        return oss.str();
    }

  private:
    token_list tokens;
};

#endif
