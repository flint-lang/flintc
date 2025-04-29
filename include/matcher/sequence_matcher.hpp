#pragma once

#include "token_pattern_matcher.hpp"
#include <sstream>

class SequenceMatcher : public TokenPatternMatcher {
  private:
    std::vector<PatternPtr> sequence;

  public:
    explicit SequenceMatcher(std::vector<PatternPtr> sequence) :
        sequence(std::move(sequence)) {}

    MatchResult match(const token_slice &tokens, size_t start_pos) const override {
        size_t current_pos = start_pos;
        for (const auto &pattern : sequence) {
            MatchResult result = pattern->match(tokens, current_pos);
            if (!result.has_value()) {
                return std::nullopt;
            }
            current_pos = *result;
        }
        return current_pos;
    }

    std::string to_string() const override {
        std::stringstream result;
        for (const auto &pattern : sequence) {
            result << pattern->to_string();
        }
        return result.str();
    }
};
