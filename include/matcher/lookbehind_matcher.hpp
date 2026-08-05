#pragma once

#include "token_pattern_matcher.hpp"

#include <string>

template <bool is_positive> class LookbehindMatcher : public TokenPatternMatcher {
  private:
    PatternPtr pattern;
    PatternPtr matcher;

  public:
    explicit LookbehindMatcher(PatternPtr &pattern, PatternPtr &matcher);

    MatchResult match(const token_slice &tokens, size_t start_pos) const override;

    std::string to_string() const override;
};

extern template class LookbehindMatcher<true>;
extern template class LookbehindMatcher<false>;
