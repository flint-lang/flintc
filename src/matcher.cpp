#include "matcher/matcher.hpp"
#include <cassert>

std::optional<uint2> Matcher::balanced_range_extraction( //
    const token_list &tokens,                            //
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
    const token_list &tokens,                              //
    const PatternPtr &inc_pattern,                         //
    const PatternPtr &dec_pattern                          //
) {
    std::vector<uint2> ranges;

    // Create a copy of tokens to modify during extraction
    token_list tokens_mut = tokens;
    unsigned int removed_tokens = 0;

    while (!tokens_mut.empty()) {
        const std::optional<uint2> next_range = balanced_range_extraction(tokens_mut, inc_pattern, dec_pattern);
        if (!next_range.has_value()) {
            break;
        }

        // Adjust the range to account for removed tokens
        const uint2 original_range = {next_range.value().first + removed_tokens, next_range.value().second + removed_tokens};
        ranges.push_back(original_range);

        // Remove everything up to and including the current balanced range
        const unsigned int erase_end = next_range.value().second + 1;
        tokens_mut.erase(tokens_mut.begin(), tokens_mut.begin() + erase_end);

        // Update removed tokens counter
        removed_tokens += erase_end;
    }

    return ranges;
}

bool Matcher::tokens_contain(const token_list &tokens, const PatternPtr &pattern) {
    for (size_t i = 0; i < tokens.size(); i++) {
        auto result = pattern->match(tokens, i);
        if (result.has_value()) {
            return true;
        }
    }
    return false;
}

bool Matcher::tokens_match(const token_list &tokens, const PatternPtr &pattern) {
    auto result = pattern->match(tokens, 0);
    return result.has_value() && *result == tokens.size();
}

std::vector<uint2> Matcher::get_match_ranges(const token_list &tokens, const PatternPtr &pattern) {
    std::vector<uint2> results;

    for (size_t i = 0; i < tokens.size(); i++) {
        auto match_end = pattern->match(tokens, i);
        if (match_end.has_value()) {
            results.emplace_back(i, *match_end);
        }
    }
    return results;
}

bool Matcher::tokens_contain_in_range(const token_list &tokens, const PatternPtr &pattern, const uint2 &range) {
    assert(range.second <= tokens.size());
    assert(range.first <= range.second);

    for (size_t i = range.first; i < range.second; i++) {
        auto result = pattern->match(tokens, i);
        if (result.has_value() && *result <= range.second) {
            return true;
        }
    }
    return false;
}

std::vector<uint2> Matcher::get_match_ranges_in_range(const token_list &tokens, const PatternPtr &pattern, const uint2 &range) {
    assert(range.second <= tokens.size());
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

std::optional<uint2> Matcher::get_next_match_range(const token_list &tokens, const PatternPtr &pattern) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        auto match_end = pattern->match(tokens, i);
        if (match_end) {
            return std::make_optional<uint2>(i, *match_end);
        }
    }
    return std::nullopt;
}

std::optional<unsigned int> Matcher::get_leading_indents(const token_list &tokens, unsigned int line) {
    unsigned int leading_indents = 0;
    int start_index = -1;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (start_index == -1 && tokens.at(i).line == line) {
            start_index = i;
            if (tokens.at(i).type == TOK_INDENT) {
                ++leading_indents;
            }
        } else if (start_index != -1) {
            if (tokens.at(i).type == TOK_INDENT) {
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

std::optional<uint2> Matcher::get_tokens_line_range(const token_list &tokens, unsigned int line) {
    if (tokens.empty()) {
        return std::nullopt;
    }

    // Find start index
    size_t start_index = 0;
    while (start_index < tokens.size() && tokens[start_index].line < line) {
        start_index++;
    }

    // If we reached the end or found a line past our target, line doesn't exist
    if (start_index >= tokens.size() || tokens[start_index].line > line) {
        return std::nullopt;
    }

    // Find end index (last token of the line)
    size_t end_index = start_index;
    while (end_index + 1 < tokens.size() && tokens[end_index + 1].line == line) {
        end_index++;
    }

    return std::make_optional<uint2>(start_index, end_index);
}
