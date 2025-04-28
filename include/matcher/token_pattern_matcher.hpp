#pragma once

#include "types.hpp"
#include <cstddef>
#include <memory>
#include <optional>

class TokenPatternMatcher {
  public:
    using MatchResult = std::optional<size_t>;

    virtual ~TokenPatternMatcher() = default;
    virtual MatchResult match(const token_list &tokens, size_t start_pos) const = 0;
    virtual std::string to_string() const = 0;
};

using PatternPtr = std::shared_ptr<TokenPatternMatcher>;
