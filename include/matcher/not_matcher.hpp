#pragma once

#include "profiler.hpp"
#include "token_pattern_matcher.hpp"

#include <string>

class NotMatcher : public TokenPatternMatcher {
  private:
    PatternPtr pattern;

  public:
    explicit NotMatcher(PatternPtr &pattern) :
        pattern(pattern) {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        PROFILE_CUMULATIVE("NotMatcher::match");
        MatchResult result = pattern->match(tokens, start_pos);
        if (result.has_value()) {
            return std::nullopt;
        } else {
            return start_pos + 1;
        }
    }

    std::string to_string() const override {
        return "NotMatcher(" + pattern->to_string() + ")";
    }
};
