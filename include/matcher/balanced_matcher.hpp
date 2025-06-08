#pragma once

#include "token_pattern_matcher.hpp"

class BalancedMatcher : public TokenPatternMatcher {
  private:
    PatternPtr increment_pattern;
    PatternPtr decrement_pattern;
    unsigned int start_depth;

  public:
    explicit BalancedMatcher(PatternPtr &increment_pattern, PatternPtr decrement_pattern, const unsigned int start_depth) :
        increment_pattern(increment_pattern),
        decrement_pattern(decrement_pattern),
        start_depth(start_depth) {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        size_t pos = start_pos;
        size_t tokens_size = std::distance(tokens.first, tokens.second);
        unsigned int depth = start_depth;

        // Find the ending pattern
        while (pos < tokens_size) {
            MatchResult result = increment_pattern->match(tokens, pos);
            if (result.has_value()) {
                depth++;
                pos = *result;
                continue;
            }

            result = decrement_pattern->match(tokens, pos);
            if (result.has_value()) {
                depth--;
                if (depth == 0) {
                    return result;
                }
                pos = *result;
                continue;
            }
            pos++;
        }
        // If we get here, we didn't find the ending pattern
        return std::nullopt;
    }

    std::string to_string() const override {
        return "BalancedMatch(" + increment_pattern->to_string() + ", " + decrement_pattern->to_string() + ")";
    }
};
