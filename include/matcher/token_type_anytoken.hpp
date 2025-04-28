#pragma once

#include "token_pattern_matcher.hpp"

class TokenTypeAnytoken : public TokenPatternMatcher {
  public:
    explicit TokenTypeAnytoken() {}

    MatchResult match(const token_list &tokens, size_t start_pos) const override {
        if (tokens.empty()) {
            return std::nullopt;
        }
        return start_pos;
    }

    std::string to_string() const override {
        return ".";
    }
};
