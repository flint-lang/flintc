#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrStmtIfChainMissingIf : public BaseError {
  public:
    ErrStmtIfChainMissingIf(const ErrorType error_type, const std::string &file, const token_list &tokens) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Expected if chain to start with 'if' statement: " << YELLOW << get_token_string(tokens, {})
            << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
};
