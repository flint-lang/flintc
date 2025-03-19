#include "parser/signature.hpp"
#include "types.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <utility>
#include <vector>

// PUBLIC FUNCTIONS

std::string Signature::stringify(const token_list &tokens) {
    std::stringstream token_string;
    for (const TokenContext &tok : tokens) {
        token_string << "#";
        token_string << std::to_string(static_cast<int>(tok.type));
        token_string << "#";
    }
    return token_string.str();
}

std::optional<uint2> Signature::balanced_range_extraction(const token_list &tokens, const signature &inc, const signature &dec) {
    const std::string inc_str = get_regex_string(inc);
    const std::string dec_str = get_regex_string(dec);
    if (!tokens_contain(tokens, inc_str) || !tokens_contain(tokens, dec_str)) {
        return std::nullopt;
    }

    std::vector<uint2> inc_ranges = get_match_ranges(tokens, inc_str);
    std::vector<uint2> dec_ranges = get_match_ranges(tokens, dec_str);
    assert(!inc_ranges.empty() && !dec_ranges.empty());
    if (inc_ranges.size() == 1 && dec_ranges.size() == 1 && inc_ranges.at(0).first < dec_ranges.at(0).first) {
        return std::make_pair(inc_ranges.at(0).first, dec_ranges.at(0).second);
    }

    auto inc_iterator = inc_ranges.begin();
    auto dec_iterator = dec_ranges.begin();

    const unsigned int first_idx = inc_iterator->first;
    ++inc_iterator;
    unsigned int balance = 1;
    unsigned int last_idx = 0;
    do {
        if (inc_iterator != inc_ranges.end() && inc_iterator->first < dec_iterator->first) {
            ++balance;
            ++inc_iterator;
        } else if (dec_iterator != dec_ranges.end()) {
            --balance;
            last_idx = dec_iterator->second;
            ++dec_iterator;
            if (balance == 0) {
                break;
            }
        }
    } while (inc_iterator != inc_ranges.end() || dec_iterator != dec_ranges.end());
    if (balance != 0) {
        return std::nullopt;
    }
    return std::make_pair(first_idx, last_idx);
}

std::vector<uint2> Signature::balanced_range_extraction_vec(const token_list &tokens, const signature &inc, const signature &dec) {
    // create a local, mutable, copy of the tokens
    token_list tokens_mut;
    tokens_mut.reserve(tokens.size());
    std::copy(tokens.begin(), tokens.end(), std::back_inserter(tokens_mut));
    std::vector<uint2> ranges;
    unsigned int removed_tokens = 0;
    while (true) {
        std::optional<uint2> next_range = balanced_range_extraction(tokens_mut, inc, dec);
        if (!next_range.has_value()) {
            break;
        }
        tokens_mut.erase(tokens_mut.begin() + next_range.value().first, tokens_mut.begin() + next_range.value().second);
        next_range.value().first += removed_tokens;
        next_range.value().second += removed_tokens;
        ranges.push_back(next_range.value());
        removed_tokens += next_range.value().second - next_range.value().first;
    }
    return ranges;
}

Signature::signature Signature::match_until_signature(const signature &signature) {
    return combine({{"((?:(?!"}, signature, {").)*"}, signature, {")"}});
}

std::string Signature::get_regex_string(const signature &sig) {
    std::stringstream command_string;
    for (const std::variant<Token, std::string> &command : sig) {
        if (const auto &tok = std::get_if<Token>(&command)) {
            command_string << "#";
            command_string << std::to_string(static_cast<int>(*tok));
            command_string << "#";
        } else if (const auto &str = std::get_if<std::string>(&command)) {
            command_string << *str;
        }
    }
    return command_string.str();
}

bool Signature::tokens_contain(const token_list &tokens, const ESignature signature) {
    return tokens_contain(tokens, get(signature));
}

bool Signature::tokens_contain(const token_list &tokens, const Token signature) {
    for (const auto &tok : tokens) {
        if (tok.type == signature) {
            return true;
        }
    }
    return false;
}

bool Signature::tokens_match(const token_list &tokens, const ESignature signature) {
    return tokens_match(tokens, get(signature));
}

bool Signature::tokens_contain_in_range(const token_list &tokens, const ESignature signature, const uint2 &range) {
    return tokens_contain_in_range(tokens, get(signature), range);
}

bool Signature::tokens_contain_in_range(const token_list &tokens, const Token signature, const uint2 &range) {
    assert(range.second <= tokens.size());
    assert(range.second > range.first);
    for (auto it = (tokens.begin() + range.first); it != (tokens.begin() + range.second + 1); ++it) {
        if (it->type == signature) {
            return true;
        }
    }
    return false;
}

std::optional<uint2> Signature::get_tokens_line_range(const token_list &tokens, unsigned int line) {
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

std::vector<uint2> Signature::get_match_ranges(const token_list &tokens, const std::string &signature) {
    std::vector<uint2> matches;
    std::string search_string = stringify(tokens);
    std::regex pattern(signature);

    auto begin = std::sregex_iterator(search_string.begin(), search_string.end(), pattern);
    auto end = std::sregex_iterator();

    for (std::sregex_iterator i = begin; i != end; ++i) {
        const std::smatch &match = *i;
        unsigned int start_idx = 0;
        unsigned int end_idx = 0;

        // Count tokens for the starting position
        std::string prefix = search_string.substr(0, match.position());
        start_idx = std::count(prefix.begin(), prefix.end(), '#') / 2;

        // Count tokens for the ending position
        std::string matched_segment = match.str();
        end_idx = start_idx + std::count(matched_segment.begin(), matched_segment.end(), '#') / 2;

        matches.emplace_back(start_idx, end_idx);
    }
    return matches;
}

std::vector<uint2> Signature::get_match_ranges(const token_list &tokens, const ESignature signature) {
    return get_match_ranges(tokens, get(signature));
}

std::vector<uint2> Signature::get_match_ranges_in_range(const token_list &tokens, const std::string &signature, const uint2 &range) {
    std::vector<uint2> match_ranges = get_match_ranges(tokens, signature);
    auto iterator = match_ranges.begin();
    while (iterator != match_ranges.end()) {
        if (iterator->first < range.first || iterator->second > range.second) {
            match_ranges.erase(iterator);
        } else {
            ++iterator;
        }
    }
    return match_ranges;
}

std::vector<uint2> Signature::get_match_ranges_in_range(const token_list &tokens, const Token signature, const uint2 &range) {
    assert(range.second <= tokens.size());
    assert(range.second > range.first);
    std::vector<uint2> match_ranges;
    unsigned int index = range.first;
    for (auto it = (tokens.begin() + range.first); it != (tokens.begin() + range.second); ++it, ++index) {
        if (it->type == signature) {
            match_ranges.emplace_back(index, index + 1);
        }
    }
    return match_ranges;
}

std::optional<uint2> Signature::get_next_match_range(const token_list &tokens, const std::string &signature) {
    if (tokens_contain(tokens, signature)) {
        // This is kinda inefficient, but it works for now
        // TODO: Rewrite this to be more efficient
        return get_match_ranges(tokens, signature).at(0);
    }
    return std::nullopt;
}

std::optional<unsigned int> Signature::get_leading_indents(const token_list &tokens, unsigned int line) {
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

// PRIVATE FUNCTIONS

Signature::signature Signature::combine(std::initializer_list<signature> signatures) {
    signature result;
    for (const auto &sig : signatures) {
        result.insert(result.end(), sig.begin(), sig.end());
    }
    return result;
}

std::string Signature::get(const ESignature signature) {
    return regex_strings.at(signature);
}

bool Signature::tokens_contain(const token_list &tokens, const std::string &signature) {
    std::string token_string = stringify(tokens);
    std::regex regex = std::regex(signature);
    std::smatch match;
    return std::regex_search(token_string, regex);
}

bool Signature::tokens_match(const token_list &tokens, const std::string &signature) {
    std::string token_string = stringify(tokens);
    std::regex regex = std::regex(signature);
    std::smatch match;
    return std::regex_match(token_string, match, regex);
}

bool Signature::tokens_contain_in_range(const token_list &tokens, const std::string &signature, const uint2 &range) {
    assert(range.second <= tokens.size());
    assert(range.second > range.first);
    std::vector<uint2> matches = get_match_ranges(tokens, signature);
    for (const auto &match : matches) {
        if (match.first >= range.first && match.second <= range.second) {
            return true;
        }
    }
    return false;
}
