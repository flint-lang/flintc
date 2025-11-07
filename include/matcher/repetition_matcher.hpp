#pragma once

#include "profiler.hpp"
#include "token_pattern_matcher.hpp"

#include <cstdint>
#include <string>

class RepetitionMatcher : public TokenPatternMatcher {
  private:
    PatternPtr pattern;
    size_t min_matches;
    size_t max_matches;

  public:
    RepetitionMatcher(PatternPtr &pattern, size_t min_matches = 0, size_t max_matches = SIZE_MAX) :
        pattern(std::move(pattern)),
        min_matches(min_matches),
        max_matches(max_matches) {}

    MatchResult match(const token_slice &tokens, const size_t start_pos) const override {
        PROFILE_CUMULATIVE("RepetitionMatcher::match");
        size_t current_pos = start_pos;
        size_t match_count = 0;
        size_t tokens_size = std::distance(tokens.first, tokens.second);

        while (match_count < max_matches && current_pos < tokens_size) {
            MatchResult result = pattern->match(tokens, current_pos);
            if (!result.has_value()) {
                break;
            }
            current_pos = *result;
            match_count++;
        }

        return match_count >= min_matches ? std::make_optional(current_pos) : std::nullopt;
    }

    std::string to_string() const override {
        std::string quantifier;
        if (min_matches == 0 && max_matches == SIZE_MAX) {
            quantifier = "*";
        } else if (min_matches == 1 && max_matches == SIZE_MAX) {
            quantifier = "+";
        } else if (min_matches == 0 && max_matches == 1) {
            quantifier = "?";
        } else {
            quantifier = "{" + std::to_string(min_matches) + "," + (max_matches == SIZE_MAX ? "" : std::to_string(max_matches)) + "}";
        }
        return "(" + pattern->to_string() + ")" + quantifier;
    }
};
