#ifndef __ERR_BODY_CREATION_FAILED_HPP__
#define __ERR_BODY_CREATION_FAILED_HPP__

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrBodyCreationFailed : public BaseError {
  public:
    ErrBodyCreationFailed(const ErrorType error_type, const std::string &file, const token_list &tokens) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Failed to parse body:\n" << YELLOW << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
};

#endif
