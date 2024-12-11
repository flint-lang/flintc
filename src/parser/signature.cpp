#include "signature.hpp"

#include "../types.hpp"

#include <regex>
#include <optional>
#include <vector>
#include <utility>
#include <cassert>

namespace Signature {
    /// combine
    ///     combines all the initializers of the signature type into a single signature type.
    ///     This is possible because the signature type is of type vector, so each signature, being a vector, can be inserted into a coherent vector of variants.
    ///     This Makes creating nested signatures **much** easier
    signature combine(std::initializer_list<signature> signatures) {
        signature result;
        for(const auto &sig : signatures) {
            result.insert(result.end(), sig.begin(), sig.end());
        }
        return result;
    }

    /// get_regex_string
    ///     Returns the built regex string of a passed in signature
    std::string get_regex_string(const signature &sig) {
        std::string command_string;
        for(const std::variant<Token, std::string> &command : sig) {
            if(const auto &tok = std::get_if<Token>(&command)) {
                command_string.append("#");
                command_string.append(std::to_string(static_cast<int>(*tok)));
                command_string.append("#");
            } else if(const auto &str = std::get_if<std::string>(&command)) {
                command_string.append(*str);
            }
        }
        return command_string;
    }

    /// tokens_contain
    ///     Checks if a given vector of TokenContext contains a given signature
    bool tokens_contain(const token_list &tokens, const Signature::signature &signature) {
        std::string token_string = stringify(tokens);
        std::regex regex = std::regex(get_regex_string(signature));
        std::smatch match;
        return std::regex_search(token_string, regex);
    }

    /// tokens_match
    ///     Checks if a given vector of TokenContext matches a given signature
    bool tokens_match(const token_list &tokens, const Signature::signature &signature) {
        std::string token_string = stringify(tokens);
        std::regex regex = std::regex(get_regex_string(signature));
        std::smatch match;
        return std::regex_match(token_string, match, regex);
    }

    /// tokens_contain_in_range
    ///     Checks if a given vector of TokenContext elements matches a given signature within a given range of the vector
    bool tokens_contain_in_range(const token_list &tokens, const signature &signature, uint2 &range) {
        assert(range.second <= tokens.size());
        std::vector<uint2> matches = get_match_ranges(tokens, signature);
        for(const auto &match : matches) {
            if(match.first >= range.first && match.second <= range.second) {
                return true;
            }
        }
        return false;
    }

    /// extract_matches
    ///     Extracts the indices of all the matches of the given signature inside the given tokens vector
    std::vector<uint2> get_match_ranges(const token_list &tokens, const Signature::signature &signature) {
        std::vector<uint2> matches;
        std::string search_string = stringify(tokens);
        std::regex pattern(get_regex_string(signature));

        auto begin = std::sregex_iterator(search_string.begin(), search_string.end(), pattern);
        auto end = std::sregex_iterator();

        for(std::sregex_iterator i = begin; i != end; ++i) {
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

    /// get_match_ranges_in_range
    ///     Returns a vector of all match ranges whic hare within a given range
    std::vector<uint2> get_match_ranges_in_range(const token_list &tokens, const signature &signature, const uint2 &range) {
        std::vector<uint2> match_ranges = get_match_ranges(tokens, signature);
        auto iterator = match_ranges.begin();
        while(iterator != match_ranges.end()) {
            if(iterator->first < range.first || iterator->second > range.second) {
                match_ranges.erase(iterator);
            }
        }
        return match_ranges;
    }

    /// get_next_match
    ///     Returns the next match range, if the token_list contains the specified signature
    ///     Returns a nullopt if the token_list does not contain the signature
    std::optional<uint2> get_next_match_range(const token_list &tokens, const signature &signature) {
        if(tokens_contain(tokens, signature)) {
            // This is kinda inefficient, but it works for now
            // TODO: Rewrite this to be more efficient
            return get_match_ranges(tokens, signature).at(0);
        }
        return std::nullopt;
    }

    /// match_until_signature
    ///     Creates a new signature where all token matches that are _not_ the given signature are matched until finnally the given signature is matched. Each "not until" match can be extracted separately.
    ///     This function will be primarily used for extracting statements ('[^;]*;') matches
    signature match_until_signature(const signature &signature) {
        return combine({
            {"((?:(?!"}, signature, {").)*"}, signature, {")"}
        });
    }

    /// stringify
    ///     makes a vector of TokenContexts to a string that can be checked by regex
    std::string stringify(const token_list &tokens) {
        std::string token_string;
        for(const TokenContext &tok : tokens) {
            token_string.append("#");
            token_string.append(std::to_string(static_cast<int>(tok.type)));
            token_string.append("#");
        }
        return token_string;
    }

    /// get_leading_indents
    ///     Returns the leading indentations at the given line inside the vector of tokens
    ///     Returns none, when the given line does not exist in the list of vectors
    std::optional<unsigned int> get_leading_indents(const token_list &tokens, unsigned int line) {
        unsigned int leading_indents = 0;
        int start_index = -1;
        for(int i = 0; i < tokens.size(); i++) {
            if(start_index == -1 && tokens.at(i).line == line) {
                start_index = i;
                if(tokens.at(i).type == TOK_INDENT) {
                    ++leading_indents;
                }
            } else if(start_index != -1) {
                if(tokens.at(i).type == TOK_INDENT) {
                    ++leading_indents;
                } else {
                    break;
                }
            }
        }
        if(start_index == -1) {
            return std::nullopt;
        }
        return leading_indents;
    }

    /// get_line_token_indices
    ///     Returns the start and end index of all the tokens inisde the given array which are at the given line.
    ///     If the line is not contained inside the token vector, a nullopt is returned
    std::optional<uint2> get_line_token_indices(const token_list &tokens, unsigned int line) {
        int start_index = -1;
        unsigned int end_index = 0;

        for(int i = 0; i < tokens.size(); i++) {
            if(start_index == 0 && tokens.at(i).line == line) {
                start_index = i;
            } else if (start_index != -1 && tokens.at(i).line != line) {
                end_index = i - 1;
                break;
            }
        }

        if(start_index == -1) {
            return std::nullopt;
        }
        return std::make_pair(start_index, end_index);
    }

    /// balanced_range_extraction
    ///     Extracts the range of the given signatures where the 'inc' signature increments the amount of 'dec' signatures needed to reach the end of the range.
    ///     This can be used to extract all operations between parenthesis, for example
    std::optional<uint2> balanced_range_extraction(const token_list &tokens, const signature &inc, const signature &dec) {
        if(!tokens_contain(tokens, inc) || !tokens_contain(tokens, dec)) {
            return std::nullopt;
        }

        std::vector<uint2> inc_ranges = get_match_ranges(tokens, inc);
        std::vector<uint2> dec_ranges = get_match_ranges(tokens, dec);
        assert(!inc_ranges.empty() && !dec_ranges.empty());
        if(inc_ranges.size() == 1 && dec_ranges.size() == 1 && inc_ranges.at(0).first < dec_ranges.at(0).first) {
            return std::make_pair(inc_ranges.at(0).first, dec_ranges.at(0).second);
        }

        auto inc_iterator = inc_ranges.begin();
        auto dec_iterator = dec_ranges.begin();

        const unsigned int first_idx = inc_iterator->first;
        ++inc_iterator;
        unsigned int balance = 1;
        unsigned int last_idx = 0;
        do {
            if(inc_iterator != inc_ranges.end() && inc_iterator->first < dec_iterator->first) {
                ++balance;
                ++inc_iterator;
            } else if (dec_iterator != dec_ranges.end()) {
                --balance;
                last_idx = dec_iterator->second;
                ++dec_iterator;
                if(balance == 0) {
                    break;
                }
            }
        } while(inc_iterator != inc_ranges.end() || dec_iterator != dec_ranges.end());
        if(balance != 0) {
            return std::nullopt;
        }
        return std::make_pair(first_idx, last_idx);
    }
}
