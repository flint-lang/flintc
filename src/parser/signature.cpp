#include "signature.hpp"
#include "../types.hpp"

#include <regex>
#include <optional>
#include <vector>
#include <utility>

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

    /// extract_matches
    ///     Extracts the indices of all the matches of the given signature inside the given tokens vector
    std::vector<std::pair<unsigned int, unsigned int>> extract_matches(const token_list &tokens, const Signature::signature &signature) {
        std::vector<std::pair<unsigned int, unsigned int>> matches;
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
    std::optional<std::pair<unsigned int, unsigned int>> get_line_token_indices(const token_list &tokens, unsigned int line) {
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
}
