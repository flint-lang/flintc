#pragma once

#include "token_pattern_matcher.hpp"

class TokenTypeMatcher : public TokenPatternMatcher {
  private:
    Token token_type;

  public:
    explicit TokenTypeMatcher(Token tok) :
        token_type(tok) {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        size_t tokens_size = std::distance(tokens.first, tokens.second);
        if (start_pos >= tokens_size) {
            return std::nullopt;
        }
        return (tokens.first + start_pos)->token == token_type ? std::make_optional(start_pos + 1) : std::nullopt;
    }

    std::string to_string() const override {
        return "Token(" + std::to_string(static_cast<int>(token_type)) + ")";
    }
};
