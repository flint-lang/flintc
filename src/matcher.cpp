#include "matcher/matcher.hpp"

#include <cassert>
#include <regex>

std::optional<uint2> Matcher::balanced_range_extraction( //
    const token_slice &tokens,                           //
    const PatternPtr &inc_pattern,                       //
    const PatternPtr &dec_pattern                        //
) {
    // Find all occurrences of increment and decrement patterns
    std::vector<uint2> inc_ranges = get_match_ranges(tokens, inc_pattern);
    std::vector<uint2> dec_ranges = get_match_ranges(tokens, dec_pattern);

    // Check if both patterns are found
    if (inc_ranges.empty() || dec_ranges.empty()) {
        return std::nullopt;
    }

    // Simple case: one increment, one decrement
    if (inc_ranges.size() == 1 && dec_ranges.size() == 1 && inc_ranges.at(0).first < dec_ranges.at(0).first) {
        return std::make_pair(inc_ranges.at(0).first, dec_ranges.at(0).second);
    }

    // More complex case: multiple increments and decrements
    auto inc_iterator = inc_ranges.begin();
    auto dec_iterator = dec_ranges.begin();

    const unsigned int first_idx = inc_iterator->first;
    ++inc_iterator;
    unsigned int balance = 1;
    unsigned int last_idx = 0;

    do {
        if (inc_iterator != inc_ranges.end() && (dec_iterator == dec_ranges.end() || inc_iterator->first < dec_iterator->first)) {
            // Found another increment
            ++balance;
            ++inc_iterator;
        } else if (dec_iterator != dec_ranges.end()) {
            // Found a decrement
            --balance;
            last_idx = dec_iterator->second;
            ++dec_iterator;
            if (balance == 0) {
                break;
            }
        }
    } while (inc_iterator != inc_ranges.end() || dec_iterator != dec_ranges.end());

    // If balance is not 0, we didn't find a balanced range
    if (balance != 0) {
        return std::nullopt;
    }

    return std::make_pair(first_idx, last_idx);
}

std::vector<uint2> Matcher::balanced_range_extraction_vec( //
    const token_slice &tokens,                             //
    const PatternPtr &inc_pattern,                         //
    const PatternPtr &dec_pattern                          //
) {
    std::vector<uint2> ranges;

    // Create a copy of tokens to modify during extraction
    token_slice tokens_mut = tokens;
    unsigned int removed_tokens = 0;

    while (tokens_mut.first != tokens_mut.second) {
        const std::optional<uint2> next_range = balanced_range_extraction(tokens_mut, inc_pattern, dec_pattern);
        if (!next_range.has_value()) {
            break;
        }

        // Adjust the range to account for removed tokens
        const uint2 original_range = {next_range.value().first + removed_tokens, next_range.value().second + removed_tokens};
        ranges.push_back(original_range);

        // Remove everything up to and including the current balanced range
        const unsigned int erase_end = next_range.value().second;
        tokens_mut.first += erase_end;

        // Update removed tokens counter
        removed_tokens += erase_end;
    }

    return ranges;
}

std::vector<uint2> Matcher::balanced_ranges_vec(const std::string &src, const std::string &inc, const std::string &dec) {
    std::vector<uint2> result;
    std::regex inc_regex(inc), dec_regex(dec);

    std::stack<size_t> stack;
    std::sregex_iterator end;
    std::vector<std::pair<size_t, bool>> positions;

    // Collect increment positions
    for (std::sregex_iterator it(src.begin(), src.end(), inc_regex); it != end; ++it) {
        size_t match_start = it->position();
        size_t match_length = it->length();
        // The { is always at the last position of the match
        size_t brace_pos = match_start + match_length - 1;
        positions.push_back({brace_pos, true});
    }

    // Collect decrement positions
    for (std::sregex_iterator it(src.begin(), src.end(), dec_regex); it != end; ++it) {
        size_t match_start = it->position();
        size_t match_length = it->length();
        // The } is always at the last position of the match
        size_t brace_pos = match_start + match_length - 1;
        positions.push_back({brace_pos, false});
    }

    // Sort positions by their location in the string
    std::sort(positions.begin(), positions.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

    // Process positions in order
    for (const auto &[pos, isIncrement] : positions) {
        if (isIncrement) {
            // Push the position of increment onto stack
            stack.push(pos);
        } else if (!stack.empty()) {
            // Found matching decrement, create a balanced range
            size_t start = stack.top();
            stack.pop();

            // Add the start and end positions as a balanced range
            // Assuming uint2 is a pair-like structure for two unsigned integers
            result.push_back({static_cast<unsigned int>(start), static_cast<unsigned int>(pos)});
        }
        // Ignore decrements with no matching increment
    }

    // Remove nested ranges (ranges that are completely contained within other ranges)
    for (auto it = result.begin(); it != result.end();) {
        bool is_nested = false;

        for (const auto &other : result) {
            // Check if current range is completely contained within another range
            if (&(*it) != &other && other.first <= it->first && it->second <= other.second) {
                is_nested = true;
                break;
            }
        }

        if (is_nested) {
            it = result.erase(it);
        } else {
            ++it;
        }
    }

    return result;
}

bool Matcher::tokens_contain(const token_slice &tokens, const PatternPtr &pattern) {
    size_t tokens_size = std::distance(tokens.first, tokens.second);
    for (size_t i = 0; i < tokens_size; i++) {
        auto result = pattern->match(tokens, i);
        if (result.has_value()) {
            return true;
        }
    }
    return false;
}

bool Matcher::tokens_match(const token_slice &tokens, const PatternPtr &pattern) {
    size_t tokens_size = std::distance(tokens.first, tokens.second);
    auto result = pattern->match(tokens, 0);
    return result.has_value() && *result == tokens_size;
}

bool Matcher::token_match(const Token token, const PatternPtr &pattern) {
    token_list match_list = {TokenContext{token, 0, 0, 0, ""}};
    return tokens_match({match_list.begin(), match_list.end()}, pattern);
}

bool Matcher::tokens_start_with(const token_slice &tokens, const PatternPtr &pattern) {
    return pattern->match(tokens, 0).has_value();
}

bool Matcher::tokens_end_with(const token_slice &tokens, const PatternPtr &pattern) {
    for (auto start = tokens.second - 1; start != tokens.first; --start) {
        auto match = pattern->match({start, tokens.second}, 0);
        if (match.has_value()) {
            return match.value() == static_cast<size_t>(std::distance(start, tokens.second));
        }
    }
    return false;
}

bool Matcher::tokens_contain_in_range(const token_slice &tokens, const PatternPtr &pattern, const uint2 &range) {
    size_t tokens_size = std::distance(tokens.first, tokens.second);
    assert(range.second <= tokens_size);
    assert(range.first <= range.second);

    for (size_t i = range.first; i < range.second; i++) {
        auto result = pattern->match(tokens, i);
        if (result.has_value() && *result <= range.second) {
            return true;
        }
    }
    return false;
}

std::optional<uint2> Matcher::get_tokens_line_range(const token_slice &tokens, unsigned int line) {
    if (tokens.first == tokens.second) {
        return std::nullopt;
    }

    // Find start index
    size_t start_index = 0;
    size_t tokens_size = std::distance(tokens.first, tokens.second);
    while (start_index < tokens_size && (tokens.first + start_index)->line < line) {
        start_index++;
    }

    // If we reached the end or found a line past our target, line doesn't exist
    if (start_index >= tokens_size || (tokens.first + start_index)->line > line) {
        return std::nullopt;
    }

    // Find end index (last token of the line)
    size_t end_index = start_index;
    while (end_index + 1 < tokens_size && (tokens.first + end_index + 1)->line == line) {
        end_index++;
    }

    return std::make_optional<uint2>(start_index, end_index);
}

std::vector<uint2> Matcher::get_match_ranges(const token_slice &tokens, const PatternPtr &pattern) {
    std::vector<uint2> results;
    size_t tokens_size = std::distance(tokens.first, tokens.second);

    for (size_t i = 0; i < tokens_size; i++) {
        auto match_end = pattern->match(tokens, i);
        if (match_end.has_value()) {
            results.emplace_back(i, *match_end);
        }
    }
    return results;
}

std::vector<uint2> Matcher::get_match_ranges_in_range(const token_slice &tokens, const PatternPtr &pattern, const uint2 &range) {
    size_t tokens_size = std::distance(tokens.first, tokens.second);
    assert(range.second <= tokens_size);
    assert(range.first <= range.second);

    std::vector<uint2> results;
    for (size_t i = range.first; i < range.second; i++) {
        auto match_end = pattern->match(tokens, i);
        if (match_end.has_value() && *match_end <= range.second) {
            results.emplace_back(i, *match_end);
            i = *match_end - 1;
        }
    }
    return results;
}

std::optional<uint2> Matcher::get_next_match_range(const token_slice &tokens, const PatternPtr &pattern) {
    size_t tokens_size = std::distance(tokens.first, tokens.second);
    for (size_t i = 0; i < tokens_size; ++i) {
        auto match_end = pattern->match(tokens, i);
        if (match_end) {
            return std::make_optional<uint2>(i, *match_end);
        }
    }
    return std::nullopt;
}

std::optional<unsigned int> Matcher::get_leading_indents(const token_slice &tokens, unsigned int line) {
    unsigned int leading_indents = 0;
    int start_index = -1;
    size_t tokens_size = std::distance(tokens.first, tokens.second);
    for (size_t i = 0; i < tokens_size; i++) {
        if (start_index == -1 && (tokens.first + i)->line == line) {
            start_index = i;
            if ((tokens.first + i)->token == TOK_INDENT) {
                ++leading_indents;
            }
        } else if (start_index != -1) {
            if ((tokens.first + i)->token == TOK_INDENT) {
                ++leading_indents;
            } else {
                break;
            }
        }
    }
    if (start_index == -1) {
        return std::nullopt;
    }
    return leading_indents;
}

std::vector<uint2> Matcher::get_match_ranges_in_range_outside_group( //
    const token_slice &tokens,                                       //
    const PatternPtr &pattern,                                       //
    const uint2 &range,                                              //
    const PatternPtr &inc,                                           //
    const PatternPtr &dec                                            //
) {
    // No need for more expensive search if the tokens dont contain the signature at all
    if (!tokens_contain_in_range(tokens, pattern, range)) {
        return {};
    }
    token_slice in_range_tokens = {tokens.first + range.first, tokens.first + range.second};
    std::vector<uint2> balanced_ranges = balanced_range_extraction_vec(in_range_tokens, inc, dec);
    // Correct all indices of the balanced ranges, range.first now should be added to them
    for (auto &balanced : balanced_ranges) {
        balanced.first += range.first;
        balanced.second += range.first;
    }
    std::vector<uint2> match_ranges = get_match_ranges_in_range(tokens, pattern, range);
    if (balanced_ranges.empty()) {
        // Just return the match ranges because the given signature is contained inside the token list and no balanced ranges exist to check
        return match_ranges;
    }
    // Now go through all matches of the given signature and check if any of the matches lies within the given balanced ranges
    // This is simply a 'match_ranges - balanced_ranges' extraction, as only the match_ranges that are _not_ within any of the
    // balanced_ranges are counted as "real" matches
    for (auto match = match_ranges.begin(); match != match_ranges.end();) {
        bool inside_any_group = false;

        for (const auto &balanced : balanced_ranges) {
            if (balanced.first <= match->first && balanced.second >= match->second) {
                inside_any_group = true;
                break;
            }
        }

        // If we found a match that's not inside any group, return true
        if (inside_any_group) {
            match = match_ranges.erase(match);
        } else {
            ++match;
        }
    }

    // All matches were inside groups
    return match_ranges;
}
