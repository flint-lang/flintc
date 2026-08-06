#include "matcher/matcher.hpp"

#include <sstream>

template <bool is_positive>
LookbehindMatcher<is_positive>::LookbehindMatcher(PatternPtr &pattern, PatternPtr &matcher) :
    pattern(pattern),
    matcher(matcher) {}

template <bool is_positive>
TokenPatternMatcher::MatchResult LookbehindMatcher<is_positive>::match(const token_slice &tokens, size_t start_pos) const {
    PROFILE_CUMULATIVE(is_positive ? "LookbehindMatcher::match" : "NegativeLookbehindMatcher::match");
    const MatchResult result = matcher->match(tokens, start_pos);
    if (!result.has_value()) {
        return std::nullopt;
    }
    bool end_with = false;
    if (start_pos > 0) {
        end_with = Matcher::tokens_end_with(token_slice{tokens.first, tokens.first + start_pos}, pattern);
    }
    if (end_with == is_positive) {
        return result;
    }
    return std::nullopt;
}

template <bool is_positive> std::string LookbehindMatcher<is_positive>::to_string() const {
    std::stringstream ss;
    ss << "(?<" << (is_positive ? "=" : "!") << pattern->to_string() << ")";
    return ss.str();
}

template class LookbehindMatcher<true>;
template class LookbehindMatcher<false>;
