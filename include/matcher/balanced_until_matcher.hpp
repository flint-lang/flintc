#pragma once

#include "profiler.hpp"
#include "token_pattern_matcher.hpp"

#include <string>

class BalancedUntilMatcher : public TokenPatternMatcher {
  private:
    PatternPtr increment_pattern;
    PatternPtr until_pattern;
    std::optional<PatternPtr> decrement_pattern;
    unsigned int start_depth;

  public:
    explicit BalancedUntilMatcher(                                  //
        PatternPtr &increment_pattern,                              //
        PatternPtr &until_pattern,                                  //
        std::optional<PatternPtr> decrement_pattern = std::nullopt, //
        const unsigned int start_depth = 1                          //
        ) :
        increment_pattern(increment_pattern),
        until_pattern(until_pattern),
        decrement_pattern(decrement_pattern),
        start_depth(start_depth) {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        PROFILE_CUMULATIVE("BalancedUntilMatcher::match");
        size_t pos = start_pos;
        size_t tokens_size = std::distance(tokens.first, tokens.second);

        unsigned int depth = start_depth;
        bool decrement_pattern_provided = decrement_pattern.has_value();

        PatternPtr local_decrement_pattern = decrement_pattern_provided ? decrement_pattern.value() : until_pattern;

        // Find the ending pattern
        while (pos < tokens_size) {
            MatchResult result = increment_pattern->match(tokens, pos);
            if (result.has_value()) {
                depth++;
                pos = *result;
                continue;
            }

            result = local_decrement_pattern->match(tokens, pos);
            if (result.has_value()) {
                depth--;
                if (depth == 0 && !decrement_pattern_provided) {
                    return result;
                }
                pos = *result;
                continue;
            }

            result = until_pattern->match(tokens, pos);
            if (result.has_value()) {
                // Only return the match if we're at the correct depth
                if (depth == 0) {
                    return result;
                }
                // Otherwise, just move past the token and continue matching
                pos = *result;
                continue;
            }
            pos++;
        }
        // If we get here, we didn't find the ending pattern
        return std::nullopt;
    }

    std::string to_string() const override {
        std::string result = "BalancedMatchUntil(" + increment_pattern->to_string() + ", ";
        if (decrement_pattern.has_value()) {
            result += decrement_pattern.value()->to_string() + ", ";
        }
        result += until_pattern->to_string() + ")";
        return result;
    }
};
