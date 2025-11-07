#pragma once

#include "profiler.hpp"
#include "token_pattern_matcher.hpp"

#include <string>

class UntilMatcher : public TokenPatternMatcher {
  private:
    PatternPtr until_pattern;

  public:
    explicit UntilMatcher(PatternPtr &until_pattern) :
        until_pattern(until_pattern) {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        PROFILE_CUMULATIVE("UntilMatcher::match");
        size_t pos = start_pos;
        size_t token_size = std::distance(tokens.first, tokens.second);

        // Find the ending pattern
        while (pos < token_size) {
            MatchResult result = until_pattern->match(tokens, pos);
            if (result.has_value()) {
                // We found the ending pattern
                return result;
            }
            // Move to the next token if no match
            pos++;
        }
        // If we get here, we didn't find the ending pattern
        return std::nullopt;
    }

    std::string to_string() const override {
        return "MatchUntil(" + until_pattern->to_string() + ")";
    }
};
