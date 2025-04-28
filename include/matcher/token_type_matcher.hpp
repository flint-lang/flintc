#pragma once

#include "token_pattern_matcher.hpp"

class TokenTypeMatcher : public TokenPatternMatcher {
  private:
    Token token_type;

  public:
    explicit TokenTypeMatcher(Token tok) :
        token_type(tok) {}

    MatchResult match(const token_list &tokens, size_t start_pos) const override {
        if (start_pos >= tokens.size()) {
            return std::nullopt;
        }
        return tokens[start_pos].type == token_type ? std::make_optional(start_pos + 1) : std::nullopt;
    }

    std::string to_string() const override {
        return "Token(" + std::to_string(static_cast<int>(token_type)) + ")";
    }
};
