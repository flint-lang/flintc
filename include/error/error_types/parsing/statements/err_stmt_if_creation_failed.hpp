#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrStmtIfCreationFailed : public BaseError {
  public:
    ErrStmtIfCreationFailed(const ErrorType error_type, const std::string &file,
        const std::vector<std::pair<token_list, token_list>> &if_chain) :
        BaseError(error_type, file, if_chain.at(0).first.at(0).line, if_chain.at(0).first.at(0).column),
        if_chain(if_chain) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        token_list tokens;
        for (const auto &[if_def, if_body] : if_chain) {
            std::copy(if_def.begin(), if_def.end(), tokens.end());
            std::copy(if_body.begin(), if_body.end(), tokens.end());
        }
        oss << BaseError::to_string() << "Failed to parse if chain: \n" << YELLOW << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    std::vector<std::pair<token_list, token_list>> if_chain;
};
