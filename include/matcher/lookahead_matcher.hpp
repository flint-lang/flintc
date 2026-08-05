#pragma once

#include "profiler.hpp"
#include "token_pattern_matcher.hpp"

#include <string>

template <bool is_positive> class LookaheadMatcher : public TokenPatternMatcher {
  private:
    PatternPtr pattern;

  public:
    explicit LookaheadMatcher(PatternPtr &pattern) :
        pattern(pattern) {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        const std::string &profile_str = is_positive ? "LookaheadMatcher::match" : "NegativeLookaheadMatcher::match";
        PROFILE_CUMULATIVE(profile_str);
        const MatchResult result = pattern->match(tokens, start_pos);
        if (result.has_value() == is_positive) {
            return start_pos;
        }
        return std::nullopt;
    }

    std::string to_string() const override {
        std::stringstream ss;
        ss << "(?" << (is_positive ? "=" : "!") << pattern->to_string() << ")";
        return ss.str();
    }
};
