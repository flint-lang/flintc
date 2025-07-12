#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrStmtIfCreationFailed : public BaseError {
  public:
    ErrStmtIfCreationFailed(                                                                            //
        const ErrorType error_type,                                                                     //
        const std::string &file, const std::vector<std::pair<token_slice, std::vector<Line>>> &if_chain //
        ) :
        BaseError(error_type, file, (if_chain.empty() ? 1 : if_chain.front().first.first->line),
            (if_chain.empty() ? 1 : if_chain.front().first.first->column)),
        if_chain(if_chain) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        token_list tokens;
        for (const auto &[if_def, if_body] : if_chain) {
            std::copy(if_def.first, if_def.second, tokens.end());
            for (const auto &line : if_body) {
                std::copy(line.tokens.first, line.tokens.second, tokens.end());
            }
        }
        oss << BaseError::to_string() << "Failed to parse if chain: \n"
            << YELLOW << get_token_string({tokens.begin(), tokens.end()}, {}) << DEFAULT;
        return oss.str();
    }

  private:
    std::vector<std::pair<token_slice, std::vector<Line>>> if_chain;
};
