#pragma once

#include "token_pattern_matcher.hpp"

class NotPrecededByMatcher : public TokenPatternMatcher {
  private:
    Token preceding_token;
    PatternPtr pattern;

  public:
    explicit NotPrecededByMatcher(Token preceding_token, PatternPtr &pattern) :
        preceding_token(preceding_token),
        pattern(pattern) {}

    MatchResult match(const token_list &tokens, size_t start_pos) const override {
        if (start_pos > 0 && tokens[start_pos - 1].type == preceding_token) {
            return std::nullopt;
        }
        return pattern->match(tokens, start_pos);
    }

    std::string to_string() const override {
        return "NotPrecededByMatcher(" + std::to_string(preceding_token) + ", " + pattern->to_string() + ")";
    }
};
