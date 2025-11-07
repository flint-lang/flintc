#pragma once

#include "profiler.hpp"
#include "token_pattern_matcher.hpp"

#include <string>

class TokenTypeAnytoken : public TokenPatternMatcher {
  public:
    explicit TokenTypeAnytoken() {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        PROFILE_CUMULATIVE("TokenTypeMatcher::match");
        if (tokens.first == tokens.second) {
            return std::nullopt;
        }
        return start_pos;
    }

    std::string to_string() const override {
        return ".";
    }
};
