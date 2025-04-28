#pragma once

#include "token_pattern_matcher.hpp"
#include <sstream>

class AlternativeMatcher : public TokenPatternMatcher {
  private:
    std::vector<PatternPtr> alternatives;

  public:
    explicit AlternativeMatcher(std::vector<PatternPtr> alternatives) :
        alternatives(std::move(alternatives)) {}

    MatchResult match(const token_list &tokens, size_t start_pos) const override {
        for (const auto &alternative : alternatives) {
            MatchResult result = alternative->match(tokens, start_pos);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    std::string to_string() const override {
        std::stringstream result;
        result << "(";
        for (size_t i = 0; i < alternatives.size(); i++) {
            if (i > 0) {
                result << "|";
            }
            result << alternatives[i]->to_string();
        }
        result << ")";
        return result.str();
    }
};
