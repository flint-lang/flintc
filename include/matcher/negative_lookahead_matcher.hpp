#pragma once

#include "token_pattern_matcher.hpp"

class NegativeLookaheadMatcher : public TokenPatternMatcher {
  private:
    PatternPtr pattern;

  public:
    explicit NegativeLookaheadMatcher(PatternPtr &pattern) :
        pattern(pattern) {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        MatchResult result = pattern->match(tokens, start_pos);
        if (!result.has_value()) {
            return start_pos;
        }
        return std::nullopt;
    }

    std::string to_string() const override {
        return "(?!" + pattern->to_string() + ")";
    }
};
