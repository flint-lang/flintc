#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrStmtDanglingElse : public BaseError {
  public:
    ErrStmtDanglingElse(const ErrorType error_type, const std::string &file, const token_list &tokens) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Dangling 'else' statemnt declared but no 'if' or 'else if' statements preceeding it: " << YELLOW
            << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
};
