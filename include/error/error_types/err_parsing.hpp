#ifndef __ERR_PARSING_HPP__
#define __ERR_PARSING_HPP__

#include "base_error.hpp"

#include <vector>

class ParsingError : public BaseError {
  public:
    ParsingError(const std::vector<std::string> &tokens, const std::string &file, int line, int column) :
        BaseError("Parsing Error", file, line, column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "\nOffending tokens: ";
        for (const auto &token : tokens) {
            oss << token << " ";
        }
        return oss.str();
    }

  private:
    std::vector<std::string> tokens;
};

#endif
