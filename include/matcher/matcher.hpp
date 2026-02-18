#pragma once

#include "lexer/token.hpp"

#include "matcher/alternative_matcher.hpp"
#include "matcher/balanced_matcher.hpp"
#include "matcher/balanced_until_matcher.hpp"
#include "matcher/balanced_valid_until_matcher.hpp"
#include "matcher/negative_lookahead_matcher.hpp"
#include "matcher/not_matcher.hpp"
#include "matcher/not_preceded_by_matcher.hpp"
#include "matcher/repetition_matcher.hpp"
#include "matcher/sequence_matcher.hpp"
#include "matcher/token_pattern_matcher.hpp"
#include "matcher/token_type_anytoken.hpp"
#include "matcher/token_type_matcher.hpp"
#include "matcher/until_matcher.hpp"
#include <cassert>
#include <memory>
#include <unordered_map>

class Matcher {
  public:
    Matcher() = delete;

    /// @function `balanced_range_extraction`
    /// @brief Extracts the range of the given patterns where the 'inc' pattern increments the amount of 'dec' patterns needed to
    /// reach the end of the range. This can be used to extract all operations between parenthesis, for example
    ///
    /// @param `tokens` The list of tokens in which to get the balanced range between the increment and decrement patterns
    /// @param `inc_pattern` The increment pattern
    /// @param `dec_pattern` The decrement pattern
    /// @return `std::optional<uint2>` The range of the balanced range, nullopt if the tokens do not contain the patterns
    static std::optional<uint2> balanced_range_extraction( //
        const token_slice &tokens,                         //
        const PatternPtr &inc_pattern,                     //
        const PatternPtr &dec_pattern                      //
    );

    /// @function `balanced_range_extraction_vec`
    /// @brief Extracts all balanced ranges of the given inc and dec patterns
    ///
    /// @param `tokens` The list of tokens in which to get the balanced ranges between the increment and decrement patterns
    /// @param `inc_pattern` The increment pattern
    /// @param `dec_pattern` The decrement pattern
    /// @return `std::vector<uint2>` A list of all ranges from the balanced ranges
    static std::vector<uint2> balanced_range_extraction_vec( //
        const token_slice &tokens,                           //
        const PatternPtr &inc_pattern,                       //
        const PatternPtr &dec_pattern                        //
    );

    /// @function `balanced_ranges_vec`
    /// @brief Returns a list of all balanced ranges in which the given increment and decrement is present inside the given string
    ///
    /// @param `src` The source string to search through
    /// @param `inc` The increment signature regex string
    /// @param `dec` The decrement signature regex string
    /// @return `std::vector<uint2>` A list of all ranges from the balanced ranges
    static std::vector<uint2> balanced_ranges_vec(const std::string &src, const std::string &inc, const std::string &dec);

    /// @function `tokens_contain`
    /// @brief Checks if a given vector of TokenContext contains a given pattern
    ///
    /// @param `tokens` The tokens to check if they contain the given pattern
    /// @param `pattern` The pattern to check
    /// @return `bool` Whether the given token list contains the given pattern
    static bool tokens_contain(const token_slice &tokens, const PatternPtr &pattern);

    /// @function `tokens_match`
    /// @brief Checks if a given vector of TokenContext matches a given pattern
    ///
    /// @param `tokens` The list of tokens to check
    /// @param `pattern` The pattern to check for
    /// @return `bool` Whether the given token list matches the given pattern
    static bool tokens_match(const token_slice &tokens, const PatternPtr &pattern);

    /// @function `tokens_start_with`
    /// @brief Checks if a given vector of TokenContext starts with a given pattern
    ///
    /// @param `tokens` The list of tokens to check
    /// @param `pattern` The pattern to check for
    /// @return `bool` Whether the given tokens start with the given pattern
    static bool tokens_start_with(const token_slice &tokens, const PatternPtr &pattern);

    /// @function `tokens_end_with`
    /// @brief Checks whether the given vector of tokens ends with a given pattern
    ///
    /// @param `tokens` The list of tokens to check
    /// @param `pattern` The pattern to check for
    /// @return `bool` Whether the given tokens end with the given pattern
    static bool tokens_end_with(const token_slice &tokens, const PatternPtr &pattern);

    /// @function `token_match`
    /// @brief Checks if a given token matches with the given pattern
    ///
    /// @param `token` The token to check
    /// @param `pattern` The pattern to check for
    /// @return `bool` Whether the given token matches the given pattern
    static bool token_match(const Token token, const PatternPtr &pattern);

    /// @function `tokens_contain_in_range`
    /// @brief Checks if a given vector of tokens contains a given pattern within a given range
    ///
    /// @param `tokens` The tokens to check
    /// @param `pattern` The pattern to search for
    /// @param `range` The range in which to search for the given pattern
    /// @return `bool` Whether the given token list contains the given pattern in the given range
    static bool tokens_contain_in_range(const token_slice &tokens, const PatternPtr &pattern, const uint2 &range);

    /// @function `get_tokens_line_range`
    /// @brief Returns the range of a given line within the tokens_list
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `line` The line from which to get the token range from
    /// @return `std::optional<uint2>` The range of all tokens in the given line, nullopt if no tokens exist in the given line
    static std::optional<uint2> get_tokens_line_range(const token_slice &tokens, unsigned int line);

    /// @function `get_match_ranges`
    /// @brief Returns a list of all match ranges of the given pattern in the given token list
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `pattern` The pattern to search for
    /// @return `std::vector<uint2>` A list of all matches of the given pattern
    static std::vector<uint2> get_match_ranges(const token_slice &tokens, const PatternPtr &pattern);

    /// @function `get_match_ranges_in_range`
    /// @brief Returns a list of all match ranges of the given pattern in the given token list that also are within a given range
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `pattern` The pattern to search for
    /// @param `range` The range in which to search for matches
    /// @return `std::vector<uint2>` A list of all matches within the given range of the given pattern
    static std::vector<uint2> get_match_ranges_in_range(const token_slice &tokens, const PatternPtr &pattern, const uint2 &range);

    /// @function `get_next_match_range`
    /// @brief Returns the next match range, if the token_slice contains the specified pattern
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `signature` The pattern to get the next match for
    /// @return `std::optional<uint2>` The next match range, nullopt if the token list doesn't contain the given pattern
    static std::optional<uint2> get_next_match_range(const token_slice &tokens, const PatternPtr &pattern);

    /// @function `get_leading_indents`
    /// @brief Returns the number of leading indents in the given line in the given list of tokens
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `line` The line in which to check for all indents
    /// @return `std::optional<unsigned int>` The number of indents in the given line, nullopt if the line doesnt exist
    static std::optional<unsigned int> get_leading_indents(const token_slice &tokens, unsigned int line);

    /// @function `get_match_ranges_in_range_outside_group`
    /// @brief Returns all matches of the given pattern within the given range thats not within any group defined by the inc and dec
    /// patterns for the balanced range extraction
    ///
    /// @details This function is primarily used to help in the extraction of arguments or types
    ///
    /// @param `tokens` The list of tokens to check
    /// @param `pattern` The pattern to search for
    /// @param `range` The total range in which to search for the pattern
    /// @param `inc` The increase pattern of possible balanced ranges inside the list of tokens to skip
    /// @param `dec` The decrease pattern of possible balanced ranges inside the list of tokens to skip
    /// @return `std::vector<uint2>` All the match ranges that are inside the given range but not inside any group
    static std::vector<uint2> get_match_ranges_in_range_outside_group( //
        const token_slice &tokens,                                     //
        const PatternPtr &pattern,                                     //
        const uint2 &range,                                            //
        const PatternPtr &inc,                                         //
        const PatternPtr &dec                                          //
    );

    /// @function `token`
    /// @brief Returns the pattern to match a single token
    ///
    /// @param `token` The token to match for
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr token(Token token) {
        assert(token_patterns.find(token) != token_patterns.end());
        return token_patterns.at(token);
    }

  private:
    /// @var `token_patterns`
    /// @brief A map that contains all patterns to match single tokens. This is absolutely necessary because otherwise every single
    /// pattern would create dozens of new token patterns. With this map, the patterns can be shared between other patterns instad
    static const inline std::unordered_map<Token, PatternPtr> token_patterns = {
        {TOK_EOF, std::make_shared<TokenTypeMatcher>(TOK_EOF)},

        // type token
        {TOK_TYPE, std::make_shared<TokenTypeMatcher>(TOK_TYPE)},

        // single character tokens
        {TOK_LEFT_PAREN, std::make_shared<TokenTypeMatcher>(TOK_LEFT_PAREN)},
        {TOK_RIGHT_PAREN, std::make_shared<TokenTypeMatcher>(TOK_RIGHT_PAREN)},
        {TOK_LEFT_BRACKET, std::make_shared<TokenTypeMatcher>(TOK_LEFT_BRACKET)},
        {TOK_RIGHT_BRACKET, std::make_shared<TokenTypeMatcher>(TOK_RIGHT_BRACKET)},
        {TOK_LEFT_BRACE, std::make_shared<TokenTypeMatcher>(TOK_LEFT_BRACE)},
        {TOK_RIGHT_BRACE, std::make_shared<TokenTypeMatcher>(TOK_RIGHT_BRACE)},
        {TOK_COMMA, std::make_shared<TokenTypeMatcher>(TOK_COMMA)},
        {TOK_DOT, std::make_shared<TokenTypeMatcher>(TOK_DOT)},
        {TOK_SEMICOLON, std::make_shared<TokenTypeMatcher>(TOK_SEMICOLON)},
        {TOK_COLON, std::make_shared<TokenTypeMatcher>(TOK_COLON)},
        {TOK_QUESTION, std::make_shared<TokenTypeMatcher>(TOK_QUESTION)},
        {TOK_EXCLAMATION, std::make_shared<TokenTypeMatcher>(TOK_EXCLAMATION)},
        {TOK_UNDERSCORE, std::make_shared<TokenTypeMatcher>(TOK_UNDERSCORE)},
        {TOK_ANNOTATION, std::make_shared<TokenTypeMatcher>(TOK_ANNOTATION)},
        {TOK_DOLLAR, std::make_shared<TokenTypeMatcher>(TOK_DOLLAR)},

        // dual character tokens
        {TOK_ARROW, std::make_shared<TokenTypeMatcher>(TOK_ARROW)},
        {TOK_PIPE, std::make_shared<TokenTypeMatcher>(TOK_PIPE)},
        {TOK_REFERENCE, std::make_shared<TokenTypeMatcher>(TOK_REFERENCE)},
        {TOK_OPT_DEFAULT, std::make_shared<TokenTypeMatcher>(TOK_OPT_DEFAULT)},
        {TOK_RANGE, std::make_shared<TokenTypeMatcher>(TOK_RANGE)},

        // arithmetic tokens
        {TOK_PLUS, std::make_shared<TokenTypeMatcher>(TOK_PLUS)},
        {TOK_MINUS, std::make_shared<TokenTypeMatcher>(TOK_MINUS)},
        {TOK_MULT, std::make_shared<TokenTypeMatcher>(TOK_MULT)},
        {TOK_DIV, std::make_shared<TokenTypeMatcher>(TOK_DIV)},
        {TOK_MOD, std::make_shared<TokenTypeMatcher>(TOK_MOD)},
        {TOK_POW, std::make_shared<TokenTypeMatcher>(TOK_POW)},

        // assign tokens
        {TOK_INCREMENT, std::make_shared<TokenTypeMatcher>(TOK_INCREMENT)},
        {TOK_DECREMENT, std::make_shared<TokenTypeMatcher>(TOK_DECREMENT)},
        {TOK_PLUS_EQUALS, std::make_shared<TokenTypeMatcher>(TOK_PLUS_EQUALS)},
        {TOK_MINUS_EQUALS, std::make_shared<TokenTypeMatcher>(TOK_MINUS_EQUALS)},
        {TOK_MULT_EQUALS, std::make_shared<TokenTypeMatcher>(TOK_MULT_EQUALS)},
        {TOK_DIV_EQUALS, std::make_shared<TokenTypeMatcher>(TOK_DIV_EQUALS)},
        {TOK_COLON_EQUAL, std::make_shared<TokenTypeMatcher>(TOK_COLON_EQUAL)},
        {TOK_EQUAL, std::make_shared<TokenTypeMatcher>(TOK_EQUAL)},

        // relational symbols
        {TOK_EQUAL_EQUAL, std::make_shared<TokenTypeMatcher>(TOK_EQUAL_EQUAL)},
        {TOK_NOT_EQUAL, std::make_shared<TokenTypeMatcher>(TOK_NOT_EQUAL)},
        {TOK_LESS, std::make_shared<TokenTypeMatcher>(TOK_LESS)},
        {TOK_LESS_EQUAL, std::make_shared<TokenTypeMatcher>(TOK_LESS_EQUAL)},
        {TOK_GREATER, std::make_shared<TokenTypeMatcher>(TOK_GREATER)},
        {TOK_GREATER_EQUAL, std::make_shared<TokenTypeMatcher>(TOK_GREATER_EQUAL)},

        // bitwise operators
        {TOK_SHIFT_LEFT, std::make_shared<TokenTypeMatcher>(TOK_SHIFT_LEFT)},
        {TOK_SHIFT_RIGHT, std::make_shared<TokenTypeMatcher>(TOK_SHIFT_RIGHT)},
        {TOK_BIT_AND, std::make_shared<TokenTypeMatcher>(TOK_BIT_AND)},
        {TOK_BIT_OR, std::make_shared<TokenTypeMatcher>(TOK_BIT_OR)},
        {TOK_BIT_XOR, std::make_shared<TokenTypeMatcher>(TOK_BIT_XOR)},
        {TOK_BIT_NEG, std::make_shared<TokenTypeMatcher>(TOK_BIT_NEG)},

        // relational keywords
        {TOK_AND, std::make_shared<TokenTypeMatcher>(TOK_AND)},
        {TOK_OR, std::make_shared<TokenTypeMatcher>(TOK_OR)},
        {TOK_NOT, std::make_shared<TokenTypeMatcher>(TOK_NOT)},

        // branching keywords
        {TOK_IF, std::make_shared<TokenTypeMatcher>(TOK_IF)},
        {TOK_ELSE, std::make_shared<TokenTypeMatcher>(TOK_ELSE)},
        {TOK_SWITCH, std::make_shared<TokenTypeMatcher>(TOK_SWITCH)},

        // looping keywords
        {TOK_FOR, std::make_shared<TokenTypeMatcher>(TOK_FOR)},
        {TOK_DO, std::make_shared<TokenTypeMatcher>(TOK_DO)},
        {TOK_WHILE, std::make_shared<TokenTypeMatcher>(TOK_WHILE)},
        {TOK_PARALLEL, std::make_shared<TokenTypeMatcher>(TOK_PARALLEL)},
        {TOK_IN, std::make_shared<TokenTypeMatcher>(TOK_IN)},
        {TOK_BREAK, std::make_shared<TokenTypeMatcher>(TOK_BREAK)},
        {TOK_CONTINUE, std::make_shared<TokenTypeMatcher>(TOK_CONTINUE)},

        // function keywords
        {TOK_DEF, std::make_shared<TokenTypeMatcher>(TOK_DEF)},
        {TOK_RETURN, std::make_shared<TokenTypeMatcher>(TOK_RETURN)},
        {TOK_FN, std::make_shared<TokenTypeMatcher>(TOK_FN)},
        {TOK_BP, std::make_shared<TokenTypeMatcher>(TOK_BP)},

        // error keywords
        {TOK_ERROR, std::make_shared<TokenTypeMatcher>(TOK_ERROR)},
        {TOK_THROW, std::make_shared<TokenTypeMatcher>(TOK_THROW)},
        {TOK_CATCH, std::make_shared<TokenTypeMatcher>(TOK_CATCH)},

        // variant keywords
        {TOK_VARIANT, std::make_shared<TokenTypeMatcher>(TOK_VARIANT)},
        {TOK_ENUM, std::make_shared<TokenTypeMatcher>(TOK_ENUM)},

        // import keywords
        {TOK_USE, std::make_shared<TokenTypeMatcher>(TOK_USE)},
        {TOK_AS, std::make_shared<TokenTypeMatcher>(TOK_AS)},
        {TOK_ALIAS, std::make_shared<TokenTypeMatcher>(TOK_ALIAS)},
        {TOK_TYPE_KEYWORD, std::make_shared<TokenTypeMatcher>(TOK_TYPE_KEYWORD)},

        // literals
        {TOK_IDENTIFIER, std::make_shared<TokenTypeMatcher>(TOK_IDENTIFIER)},

        // primitives
        {TOK_VOID, std::make_shared<TokenTypeMatcher>(TOK_VOID)},
        {TOK_BOOL, std::make_shared<TokenTypeMatcher>(TOK_BOOL)},
        {TOK_U8, std::make_shared<TokenTypeMatcher>(TOK_U8)},
        {TOK_U8X2, std::make_shared<TokenTypeMatcher>(TOK_U8X2)},
        {TOK_U8X3, std::make_shared<TokenTypeMatcher>(TOK_U8X3)},
        {TOK_U8X4, std::make_shared<TokenTypeMatcher>(TOK_U8X4)},
        {TOK_U8X8, std::make_shared<TokenTypeMatcher>(TOK_U8X8)},
        {TOK_STR, std::make_shared<TokenTypeMatcher>(TOK_STR)},
        {TOK_FLINT, std::make_shared<TokenTypeMatcher>(TOK_FLINT)},
        {TOK_U32, std::make_shared<TokenTypeMatcher>(TOK_U32)},
        {TOK_I32, std::make_shared<TokenTypeMatcher>(TOK_I32)},
        {TOK_BOOL8, std::make_shared<TokenTypeMatcher>(TOK_BOOL8)},
        {TOK_I32X2, std::make_shared<TokenTypeMatcher>(TOK_I32X2)},
        {TOK_I32X3, std::make_shared<TokenTypeMatcher>(TOK_I32X3)},
        {TOK_I32X4, std::make_shared<TokenTypeMatcher>(TOK_I32X4)},
        {TOK_I32X8, std::make_shared<TokenTypeMatcher>(TOK_I32X8)},
        {TOK_U64, std::make_shared<TokenTypeMatcher>(TOK_U64)},
        {TOK_I64, std::make_shared<TokenTypeMatcher>(TOK_I64)},
        {TOK_I64X2, std::make_shared<TokenTypeMatcher>(TOK_I64X2)},
        {TOK_I64X3, std::make_shared<TokenTypeMatcher>(TOK_I64X3)},
        {TOK_I64X4, std::make_shared<TokenTypeMatcher>(TOK_I64X4)},
        {TOK_F32, std::make_shared<TokenTypeMatcher>(TOK_F32)},
        {TOK_F32X2, std::make_shared<TokenTypeMatcher>(TOK_F32X2)},
        {TOK_F32X3, std::make_shared<TokenTypeMatcher>(TOK_F32X3)},
        {TOK_F32X4, std::make_shared<TokenTypeMatcher>(TOK_F32X4)},
        {TOK_F32X8, std::make_shared<TokenTypeMatcher>(TOK_F32X8)},
        {TOK_F64, std::make_shared<TokenTypeMatcher>(TOK_F64)},
        {TOK_F64X2, std::make_shared<TokenTypeMatcher>(TOK_F64X2)},
        {TOK_F64X3, std::make_shared<TokenTypeMatcher>(TOK_F64X3)},
        {TOK_F64X4, std::make_shared<TokenTypeMatcher>(TOK_F64X4)},

        // literals
        {TOK_STR_VALUE, std::make_shared<TokenTypeMatcher>(TOK_STR_VALUE)},
        {TOK_INT_VALUE, std::make_shared<TokenTypeMatcher>(TOK_INT_VALUE)},
        {TOK_FLOAT_VALUE, std::make_shared<TokenTypeMatcher>(TOK_FLOAT_VALUE)},
        {TOK_CHAR_VALUE, std::make_shared<TokenTypeMatcher>(TOK_CHAR_VALUE)},

        // builtin values
        {TOK_TRUE, std::make_shared<TokenTypeMatcher>(TOK_TRUE)},
        {TOK_FALSE, std::make_shared<TokenTypeMatcher>(TOK_FALSE)},
        {TOK_NONE, std::make_shared<TokenTypeMatcher>(TOK_NONE)},

        // data keywords
        {TOK_DATA, std::make_shared<TokenTypeMatcher>(TOK_DATA)},
        {TOK_SHARED, std::make_shared<TokenTypeMatcher>(TOK_SHARED)},
        {TOK_IMMUTABLE, std::make_shared<TokenTypeMatcher>(TOK_IMMUTABLE)},
        {TOK_ALIGNED, std::make_shared<TokenTypeMatcher>(TOK_ALIGNED)},

        // func keywords
        {TOK_FUNC, std::make_shared<TokenTypeMatcher>(TOK_FUNC)},
        {TOK_REQUIRES, std::make_shared<TokenTypeMatcher>(TOK_REQUIRES)},

        // entity keywords
        {TOK_ENTITY, std::make_shared<TokenTypeMatcher>(TOK_ENTITY)},
        {TOK_EXTENDS, std::make_shared<TokenTypeMatcher>(TOK_EXTENDS)},
        {TOK_LINK, std::make_shared<TokenTypeMatcher>(TOK_LINK)},

        // threading keywords
        {TOK_SPAWN, std::make_shared<TokenTypeMatcher>(TOK_SPAWN)},
        {TOK_SYNC, std::make_shared<TokenTypeMatcher>(TOK_SYNC)},
        {TOK_LOCK, std::make_shared<TokenTypeMatcher>(TOK_LOCK)},

        // modifiers
        {TOK_CONST, std::make_shared<TokenTypeMatcher>(TOK_CONST)},
        {TOK_MUT, std::make_shared<TokenTypeMatcher>(TOK_MUT)},
        {TOK_PERSISTENT, std::make_shared<TokenTypeMatcher>(TOK_PERSISTENT)},

        // test keywords
        {TOK_TEST, std::make_shared<TokenTypeMatcher>(TOK_TEST)},

        // fip tokens
        {TOK_EXTERN, std::make_shared<TokenTypeMatcher>(TOK_EXTERN)},
        {TOK_EXPORT, std::make_shared<TokenTypeMatcher>(TOK_EXPORT)},

        // other tokens
        {TOK_INDENT, std::make_shared<TokenTypeMatcher>(TOK_INDENT)},
        {TOK_EOL, std::make_shared<TokenTypeMatcher>(TOK_EOL)},
    };

    /// @function `one_of`
    /// @brief Returns the pattern to match one of the given patterns
    ///
    /// @param `alternatives` The alternatives, where at least one of them needs to be matched
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr one_of(std::initializer_list<PatternPtr> alternatives) {
        return std::make_shared<AlternativeMatcher>(std::vector<PatternPtr>(alternatives));
    }

    /// @function `match_until`
    /// @brief Returns the pattern to match until the given tokens
    ///
    /// @param `tokens` The tokens to match until
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr one_of(std::initializer_list<Token> tokens) {
        std::vector<PatternPtr> alts;
        for (auto tok : tokens) {
            alts.push_back(token(tok));
        }
        return std::make_shared<AlternativeMatcher>(alts);
    }

    /// @function `sequence`
    /// @brief Returns the pattern to match a token sequence
    ///
    /// @param `sequence` The pattern sequence to match for
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr sequence(std::initializer_list<PatternPtr> sequence) {
        return std::make_shared<SequenceMatcher>(std::vector<PatternPtr>(sequence));
    }

    /// @function `zero_or_more`
    /// @brief Returns the pattern to match the given pattern zero or more times
    ///
    /// @param `pattern` The pattern to match zero or more times
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr zero_or_more(PatternPtr pattern) {
        return std::make_shared<RepetitionMatcher>(pattern);
    }

    /// @function `one_or_more`
    /// @brief Returns the pattern to match the given pattern one or more times
    ///
    /// @param `pattern` The pattern to match one or more times
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr one_or_more(PatternPtr pattern) {
        return std::make_shared<RepetitionMatcher>(pattern, 1);
    }

    /// @function `two_or_more`
    /// @brief Returns the pattern to match the given pattern two or more times
    ///
    /// @param `pattern` The pattern to match two or more times
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr two_or_more(PatternPtr pattern) {
        return std::make_shared<RepetitionMatcher>(pattern, 2);
    }

    /// @function `optional`
    /// @brief Returns the pattern to optionally match the given pattern
    ///
    /// @param `pattern` The pattern to optionally match
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr optional(PatternPtr pattern) {
        return std::make_shared<RepetitionMatcher>(pattern, 0, 1);
    }

    /// @function `not_followed_by`
    /// @brief Returns the pattern to not be followed by the given pattern
    ///
    /// @param `pattern` The pattern that should not follow
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr not_followed_by(PatternPtr pattern) {
        return std::make_shared<NegativeLookaheadMatcher>(pattern);
    }

    static inline PatternPtr not_preceded_by(Token preceding_token, PatternPtr pattern) {
        return std::make_shared<NotPrecededByMatcher>(preceding_token, pattern);
    }

    /// @function `not_p`
    /// @brief Returns the pattern to not match the given pattern
    ///
    /// @param `pattern` The pattern that should not be matched
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr not_p(PatternPtr pattern) {
        return std::make_shared<NotMatcher>(pattern);
    }

    /// @function `match_until`
    /// @brief Returns the pattern to match until the given pattern
    ///
    /// @param `until_pattern` The pattern to match until
    /// @return `PatternPtr` The created pattern
    static inline PatternPtr match_until(PatternPtr until_pattern) {
        return std::make_shared<UntilMatcher>(until_pattern);
    }

    /// @function `balanced_match`
    /// @brief Returns the pattern to match the given pattern balanced
    ///
    /// @param `increment_pattern` The pattern where the balance depth increases
    /// @param `decrement_pattern` The pattern where the balance depth decreases
    /// @param `start_depth` The depth to start at
    /// @return `PatternPtr` The created pattern
    static PatternPtr balanced_match(PatternPtr increment_pattern, PatternPtr decrement_pattern, const unsigned int start_depth) {
        return std::make_shared<BalancedMatcher>(increment_pattern, decrement_pattern, start_depth);
    }

    /// @function `balanced_match_until`
    /// @brief Returns the pattern to match the given patterns balanced
    ///
    /// @param `increment_pattern` The pattern where the balance depth increases
    /// @param `until_pattern` The pattern to match until
    /// @param `decrement_pattern` The pattern where the balance depth decreases. If this is nullopt, the `until_pattern` will be both the
    /// end and the decrementor
    /// @param `start_depth` The depth to start at
    /// @return `PatternPtr` The created pattern
    static PatternPtr balanced_match_until(          //
        PatternPtr increment_pattern,                //
        PatternPtr until_pattern,                    //
        std::optional<PatternPtr> decrement_pattern, //
        const unsigned int start_depth               //
    ) {
        return std::make_shared<BalancedUntilMatcher>(increment_pattern, until_pattern, decrement_pattern, start_depth);
    }

    /// @function `balanced_match_valid_until`
    /// @brief Returns the pattern to match the given patterns balanced while also requiring that all tokens outside any group must be a
    /// valid pattern
    ///
    /// @param `increment_pattern` The pattern where the balance depth increases
    /// @param `until_pattern` The pattern to match until
    /// @param `valid_pattern` The pattern to require all tokens outside of groups match, note that this should be a match for single tokens
    /// @param `decrement_pattern` The pattern where the balance depth decreases. If this is nullopt, the `until_pattern` will be both the
    /// end and the decrementor
    /// @param `start_depth` The depth to start at
    /// @return `PatternPtr` The created pattern
    static PatternPtr balanced_match_valid_until(    //
        PatternPtr increment_pattern,                //
        PatternPtr until_pattern,                    //
        PatternPtr valid_pattern,                    //
        std::optional<PatternPtr> decrement_pattern, //
        const unsigned int start_depth               //
    ) {
        return std::make_shared<BalancedValidUntilMatcher>(increment_pattern, until_pattern, valid_pattern, decrement_pattern, start_depth);
    }

  public:
    static const inline PatternPtr anytoken = std::make_shared<TokenTypeAnytoken>();
    static const inline PatternPtr type_prim = one_of({
        token(TOK_I32), token(TOK_I64), token(TOK_U32), token(TOK_U64), token(TOK_F32),  //
        token(TOK_F64), token(TOK_FLINT), token(TOK_STR), token(TOK_U8), token(TOK_BOOL) //
    });
    static const inline PatternPtr type_prim_mult = one_of({
        token(TOK_BOOL8), token(TOK_U8X2), token(TOK_U8X3), token(TOK_U8X4), token(TOK_U8X8),                                         //
        token(TOK_I32X2), token(TOK_I32X3), token(TOK_I32X4), token(TOK_I32X8), token(TOK_I64X2), token(TOK_I64X3), token(TOK_I64X4), //
        token(TOK_F32X2), token(TOK_F32X3), token(TOK_F32X4), token(TOK_F32X8), token(TOK_F64X2), token(TOK_F64X3), token(TOK_F64X4)  //
    });
    static const inline PatternPtr literal = one_of({
        token(TOK_STR_VALUE), token(TOK_INT_VALUE), token(TOK_FLOAT_VALUE), token(TOK_CHAR_VALUE), token(TOK_TRUE), token(TOK_FALSE),
        token(TOK_NONE) //
    });
    static const inline PatternPtr simple_type = one_of({token(TOK_IDENTIFIER), type_prim, type_prim_mult});
    static const inline PatternPtr type = one_of({
        sequence({
            one_of({token(TOK_TYPE), simple_type, token(TOK_DATA), token(TOK_VARIANT)}),                                 // Single base type
            optional(sequence({token(TOK_LESS), balanced_match(token(TOK_LESS), token(TOK_GREATER), 1)})),               // <..> Type group
            zero_or_more(sequence({token(TOK_LEFT_BRACKET), zero_or_more(token(TOK_COMMA)), token(TOK_RIGHT_BRACKET)})), // [][,][,,] Arrays
            optional(one_of({
                token(TOK_QUESTION), // ? for optionals
                token(TOK_MULT)      // * for pointers
            }))                      //
        }),                          //
        token(TOK_TYPE)              //
    });

    // Symbols
    static const inline PatternPtr symbol_single = one_of({
        token(TOK_LEFT_PAREN),
        token(TOK_RIGHT_PAREN),
        token(TOK_LEFT_BRACKET),
        token(TOK_RIGHT_BRACKET),
        token(TOK_LEFT_BRACE),
        token(TOK_RIGHT_BRACE),
        token(TOK_COMMA),
        token(TOK_DOT),
        token(TOK_SEMICOLON),
        token(TOK_COLON),
        token(TOK_QUESTION),
        token(TOK_EXCLAMATION),
        token(TOK_UNDERSCORE),
        token(TOK_ANNOTATION),
        token(TOK_DOLLAR),
    });
    static const inline PatternPtr symbol_dual = one_of({
        token(TOK_ARROW),
        token(TOK_PIPE),
        token(TOK_REFERENCE),
        token(TOK_OPT_DEFAULT),
    });
    static const inline PatternPtr symbol_arithmetic = one_of({
        token(TOK_PLUS),
        token(TOK_MINUS),
        token(TOK_MULT),
        token(TOK_DIV),
        token(TOK_MOD),
        token(TOK_POW),
    });
    static const inline PatternPtr symbol_assign = one_of({
        token(TOK_INCREMENT),
        token(TOK_DECREMENT),
        token(TOK_PLUS_EQUALS),
        token(TOK_MINUS_EQUALS),
        token(TOK_MULT_EQUALS),
        token(TOK_DIV_EQUALS),
        token(TOK_COLON_EQUAL),
        token(TOK_EQUAL),
    });
    static const inline PatternPtr symbol_relational = one_of({
        token(TOK_EQUAL_EQUAL),
        token(TOK_NOT_EQUAL),
        token(TOK_LESS),
        token(TOK_LESS_EQUAL),
        token(TOK_GREATER),
        token(TOK_GREATER_EQUAL),
    });
    static const inline PatternPtr symbol_bitwise = one_of({
        token(TOK_SHIFT_LEFT),
        token(TOK_SHIFT_RIGHT),
        token(TOK_BIT_AND),
        token(TOK_BIT_OR),
        token(TOK_BIT_XOR),
        token(TOK_BIT_NEG),
    });
    static const inline PatternPtr symbol = one_of({
        symbol_single,
        symbol_dual,
        symbol_arithmetic,
        symbol_assign,
        symbol_relational,
        symbol_bitwise,
    });

    // Keywords
    static const inline PatternPtr keyword_relational = one_of({token(TOK_AND), token(TOK_OR), token(TOK_NOT)});
    static const inline PatternPtr keyword_branching = one_of({token(TOK_IF), token(TOK_ELSE), token(TOK_SWITCH)});
    static const inline PatternPtr keyword_looping = one_of({
        token(TOK_FOR),
        token(TOK_WHILE),
        token(TOK_PARALLEL),
        token(TOK_IN),
        token(TOK_BREAK),
        token(TOK_CONTINUE),
    });
    static const inline PatternPtr keyword_function = one_of({token(TOK_DEF), token(TOK_RETURN), token(TOK_FN), token(TOK_BP)});
    static const inline PatternPtr keyword_error = one_of({token(TOK_ERROR), token(TOK_THROW), token(TOK_CATCH)});
    static const inline PatternPtr keyword_variant = one_of({token(TOK_VARIANT), token(TOK_ENUM)});
    static const inline PatternPtr keyword_import = one_of({token(TOK_USE), token(TOK_AS)});
    static const inline PatternPtr keyword_data = one_of({token(TOK_DATA), token(TOK_SHARED), token(TOK_IMMUTABLE), token(TOK_ALIGNED)});
    static const inline PatternPtr keyword_func = one_of({token(TOK_FUNC), token(TOK_REQUIRES)});
    static const inline PatternPtr keyword_entity = one_of({token(TOK_ENTITY), token(TOK_EXTENDS), token(TOK_LINK)});
    static const inline PatternPtr keyword_threading = one_of({token(TOK_SPAWN), token(TOK_SYNC), token(TOK_LOCK)});
    static const inline PatternPtr keyword_modifiers = one_of({token(TOK_CONST), token(TOK_MUT), token(TOK_PERSISTENT)});
    static const inline PatternPtr keyword_test = token(TOK_TEST);
    static const inline PatternPtr keyword = one_of({
        keyword_function,
        keyword_error,
        keyword_variant,
        keyword_import,
        keyword_data,
        keyword_func,
        keyword_entity,
        keyword_threading,
        keyword_modifiers,
        keyword_test,
    });

    static const inline PatternPtr assignment_shorthand_operator = one_of({
        token(TOK_PLUS_EQUALS), token(TOK_MINUS_EQUALS), token(TOK_MULT_EQUALS), token(TOK_DIV_EQUALS) //
    });
    static const inline PatternPtr operational_binop = one_of({
        token(TOK_PLUS), token(TOK_MINUS), token(TOK_MULT), token(TOK_DIV), token(TOK_POW), token(TOK_OPT_DEFAULT) //
    });
    static const inline PatternPtr relational_binop = one_of({token(TOK_EQUAL_EQUAL), token(TOK_NOT_EQUAL), token(TOK_LESS),
        token(TOK_LESS_EQUAL), token(TOK_GREATER), token(TOK_GREATER_EQUAL)});
    static const inline PatternPtr boolean_binop = one_of({token(TOK_AND), token(TOK_OR)});
    static const inline PatternPtr binary_operator = one_of({operational_binop, relational_binop, boolean_binop});
    static const inline PatternPtr unary_operator = one_of({
        token(TOK_INCREMENT), token(TOK_DECREMENT), token(TOK_NOT), token(TOK_MINUS), token(TOK_BIT_AND) //
    });
    static const inline PatternPtr inbetween_operator = one_of({token(TOK_QUESTION), token(TOK_EXCLAMATION)});
    static const inline PatternPtr reference = sequence({
        token(TOK_IDENTIFIER), one_or_more(sequence({token(TOK_REFERENCE), token(TOK_IDENTIFIER)})) //
    });
    static const inline PatternPtr args = sequence({
        type, token(TOK_IDENTIFIER), zero_or_more(sequence({token(TOK_COMMA), type, token(TOK_IDENTIFIER)})) //
    });
    static const inline PatternPtr param = sequence({optional(one_of({token(TOK_MUT), token(TOK_CONST)})), type, token(TOK_IDENTIFIER)});
    static const inline PatternPtr params = sequence({param, zero_or_more(sequence({token(TOK_COMMA), param}))});
    static const inline PatternPtr no_prim_args = sequence({
        token(TOK_IDENTIFIER), token(TOK_IDENTIFIER),                                            //
        zero_or_more(sequence({token(TOK_COMMA), token(TOK_IDENTIFIER), token(TOK_IDENTIFIER)})) //
    });
    static const inline PatternPtr group = sequence({
        token(TOK_LEFT_PAREN), type, zero_or_more(sequence({token(TOK_COMMA), type})), token(TOK_RIGHT_PAREN) //
    });

    // --- UNTILS ---
    static const inline PatternPtr until_right_paren = balanced_match_until(token(TOK_LEFT_PAREN), token(TOK_RIGHT_PAREN), std::nullopt, 1);
    static const inline PatternPtr until_right_brace = balanced_match_until(token(TOK_LEFT_BRACE), token(TOK_RIGHT_BRACE), std::nullopt, 1);
    static const inline PatternPtr until_right_bracket = balanced_match_until(     //
        token(TOK_LEFT_PAREN), token(TOK_RIGHT_BRACKET), token(TOK_RIGHT_PAREN), 0 //
    );
    static const inline PatternPtr until_comma = balanced_match_until(                                                              //
        one_of({token(TOK_LEFT_PAREN), token(TOK_LESS)}), token(TOK_COMMA), one_of({token(TOK_RIGHT_PAREN), token(TOK_GREATER)}), 0 //
    );
    static const inline PatternPtr until_colon = match_until(token(TOK_COLON));
    static const inline PatternPtr until_arrow = match_until(token(TOK_ARROW));
    static const inline PatternPtr until_semicolon = match_until(token(TOK_SEMICOLON));
    static const inline PatternPtr until_colon_equal = match_until(token(TOK_COLON_EQUAL));
    static const inline PatternPtr until_eq_or_colon_equal = match_until(one_of({token(TOK_EQUAL), token(TOK_COLON_EQUAL)}));
    static const inline PatternPtr until_col_or_semicolon = match_until(one_of({token(TOK_COLON), token(TOK_SEMICOLON)}));

    // --- DEFINITIONS ---
    static const inline PatternPtr use_reference = sequence({
        token(TOK_IDENTIFIER), zero_or_more(sequence({token(TOK_DOT), token(TOK_IDENTIFIER)})) //
    });
    static const inline PatternPtr use_statement = sequence({token(TOK_USE), one_of({token(TOK_STR_VALUE), use_reference})});
    static const inline PatternPtr type_alias = sequence({token(TOK_TYPE_KEYWORD), token(TOK_IDENTIFIER), type});
    static const inline PatternPtr extern_function_declaration = sequence({
        token(TOK_EXTERN), token(TOK_DEF), token(TOK_IDENTIFIER), token(TOK_LEFT_PAREN), optional(params), token(TOK_RIGHT_PAREN), //
        optional(one_of({
            sequence({token(TOK_ARROW), group}), //
            sequence({token(TOK_ARROW), type})   //
        })),                                     //
        token(TOK_SEMICOLON)                     //
    });
    static const inline PatternPtr function_definition = sequence({
        optional(token(TOK_ALIGNED)), optional(token(TOK_CONST)), token(TOK_DEF),               //
        token(TOK_IDENTIFIER), token(TOK_LEFT_PAREN), optional(params), token(TOK_RIGHT_PAREN), //
        optional(one_of({
            sequence({token(TOK_ARROW), group}),                        //
            sequence({token(TOK_ARROW), type})                          //
        })),                                                            //
        optional(sequence({token(TOK_LEFT_BRACE), until_right_brace})), //
        token(TOK_COLON)                                                //
    });
    static const inline PatternPtr data_definition = sequence({
        optional(one_of({token(TOK_SHARED), token(TOK_IMMUTABLE)})), optional(token(TOK_ALIGNED)), //
        token(TOK_DATA), token(TOK_IDENTIFIER), token(TOK_COLON)                                   //
    });
    static const inline PatternPtr func_definition = sequence({
        token(TOK_FUNC), token(TOK_IDENTIFIER),                                                                                  //
        optional(sequence({token(TOK_REQUIRES), token(TOK_LEFT_PAREN), no_prim_args, token(TOK_RIGHT_PAREN)})), token(TOK_COLON) //
    });
    static const inline PatternPtr error_definition = sequence({
        token(TOK_ERROR), token(TOK_IDENTIFIER),                        //
        optional(sequence({token(TOK_LEFT_PAREN), until_right_paren})), //
        token(TOK_COLON)                                                //
    });
    static const inline PatternPtr enum_definition = sequence({token(TOK_ENUM), token(TOK_IDENTIFIER), token(TOK_COLON)});
    static const inline PatternPtr variant_definition = sequence({token(TOK_VARIANT), token(TOK_IDENTIFIER), token(TOK_COLON)});
    static const inline PatternPtr test_definition = sequence({token(TOK_TEST), token(TOK_STR_VALUE), token(TOK_COLON)});

    // --- ENTITY DEFINITION ---
    static const inline PatternPtr entity_definition = sequence({
        token(TOK_ENTITY), token(TOK_IDENTIFIER),                                                                               //
        optional(sequence({token(TOK_EXTENDS), token(TOK_LEFT_PAREN), no_prim_args, token(TOK_RIGHT_PAREN)})), token(TOK_COLON) //
    });
    static const inline PatternPtr entity_body_data = sequence({
        token(TOK_DATA), token(TOK_COLON), zero_or_more(anytoken), token(TOK_IDENTIFIER),       //
        zero_or_more(sequence({token(TOK_COMMA), token(TOK_IDENTIFIER)})), token(TOK_SEMICOLON) //
    });
    static const inline PatternPtr entity_body_func = sequence({
        token(TOK_FUNC), token(TOK_COLON), zero_or_more(anytoken), token(TOK_IDENTIFIER),       //
        zero_or_more(sequence({token(TOK_COMMA), token(TOK_IDENTIFIER)})), token(TOK_SEMICOLON) //
    });
    static const inline PatternPtr entity_body_link = sequence({reference, token(TOK_ARROW), reference, token(TOK_SEMICOLON)});
    static const inline PatternPtr entity_body_links = sequence({
        token(TOK_LINK), token(TOK_COLON), zero_or_more(anytoken), one_or_more(sequence({entity_body_link, zero_or_more(anytoken)})) //
    });
    static const inline PatternPtr entity_body_constructor = sequence({
        token(TOK_IDENTIFIER), token(TOK_LEFT_PAREN),                                                                   //
        optional(sequence({token(TOK_IDENTIFIER), zero_or_more(sequence({token(TOK_COMMA), token(TOK_IDENTIFIER)}))})), //
        token(TOK_RIGHT_PAREN), token(TOK_SEMICOLON)                                                                    //
    });
    static const inline PatternPtr entity_body = sequence({
        optional(entity_body_data), zero_or_more(anytoken), optional(entity_body_func), zero_or_more(anytoken), //
        optional(entity_body_links), zero_or_more(anytoken), entity_body_constructor                            //
    });

    // --- EXPRESSIONS ---
    static const inline PatternPtr string_interpolation = sequence({token(TOK_DOLLAR), token(TOK_STR_VALUE)});
    static const inline PatternPtr group_expression = sequence({
        token(TOK_LEFT_PAREN), balanced_match_until(token(TOK_LEFT_PAREN), token(TOK_COMMA), token(TOK_RIGHT_PAREN), 0), //
        until_right_paren                                                                                                //
    });
    static const inline PatternPtr function_call = sequence({token(TOK_IDENTIFIER), token(TOK_LEFT_PAREN), until_right_paren});
    static const inline PatternPtr instance_call = sequence({token(TOK_IDENTIFIER), token(TOK_DOT), function_call});
    static const inline PatternPtr aliased_function_call = sequence({
        one_of({token(TOK_ALIAS), token(TOK_TYPE)}), token(TOK_DOT), function_call //
    });
    static const inline PatternPtr type_cast = sequence({one_of({type_prim, token(TOK_TYPE)}), token(TOK_LEFT_PAREN), until_right_paren});
    static const inline PatternPtr bin_op_expr = sequence({
        one_or_more(not_p(binary_operator)), binary_operator, one_or_more(not_p(binary_operator)) //
    });
    static const inline PatternPtr unary_op_expr = one_of({
        sequence({one_or_more(not_p(unary_operator)), unary_operator}), sequence({unary_operator, one_or_more(not_p(unary_operator))}) //
    });
    static const inline PatternPtr literal_expr = one_of({
        sequence({literal, zero_or_more(sequence({binary_operator, literal}))}), sequence({unary_operator, literal}), //
        sequence({literal, unary_operator})                                                                           //
    });
    static const inline PatternPtr variable_expr = sequence({token(TOK_IDENTIFIER), not_followed_by(token(TOK_LEFT_PAREN))});
    static const inline PatternPtr type_field_access = sequence({token(TOK_TYPE), token(TOK_DOT), token(TOK_IDENTIFIER)});
    static const inline PatternPtr data_access = sequence({
        token(TOK_IDENTIFIER), token(TOK_DOT), one_of({token(TOK_IDENTIFIER), sequence({token(TOK_DOLLAR), token(TOK_INT_VALUE)})}), //
        not_followed_by(token(TOK_LEFT_PAREN))                                                                                       //
    });
    static const inline PatternPtr grouped_data_access = sequence({
        one_of({token(TOK_IDENTIFIER), token(TOK_TYPE)}), token(TOK_DOT), group_expression //
    });
    static const inline PatternPtr array_initializer = sequence({
        type, token(TOK_LEFT_BRACKET),                                                                                // T[ sizes ]
        one_or_more(balanced_match_until(                                                                             //
            token(TOK_LEFT_PAREN), one_of({token(TOK_COMMA), token(TOK_RIGHT_BRACKET)}), token(TOK_RIGHT_PAREN), 0)), //
        token(TOK_LEFT_PAREN), until_right_paren                                                                      // ( initializer )
    });
    static const inline PatternPtr array_access = sequence({token(TOK_IDENTIFIER), token(TOK_LEFT_BRACKET), until_right_bracket});
    static const inline PatternPtr stacked_array_access = sequence({
        array_access, one_or_more({sequence({token(TOK_LEFT_BRACKET), until_right_bracket})}) //
    });
    static const inline PatternPtr optional_chain = sequence({token(TOK_QUESTION), not_followed_by(token(TOK_LEFT_PAREN))});
    static const inline PatternPtr optional_unwrap = sequence({token(TOK_EXCLAMATION), not_followed_by(token(TOK_LEFT_PAREN))});
    static const inline PatternPtr variant_extraction = sequence({token(TOK_QUESTION), token(TOK_LEFT_PAREN), until_right_paren});
    static const inline PatternPtr variant_unwrap = sequence({token(TOK_EXCLAMATION), token(TOK_LEFT_PAREN), until_right_paren});
    static const inline PatternPtr stackable_basic_expr = one_of({
        // Method call: .call()
        sequence({token(TOK_DOT), token(TOK_IDENTIFIER), token(TOK_LEFT_PAREN), until_right_paren}),
        // Grouped access: .()
        sequence({token(TOK_DOT), token(TOK_LEFT_PAREN), until_right_paren}),
        // Field access: .field
        sequence({token(TOK_DOT), token(TOK_IDENTIFIER)}),
        // Tuple / multi-type field access: .$N
        sequence({token(TOK_DOT), token(TOK_DOLLAR), token(TOK_INT_VALUE)}),
        // Array/map access: []
        sequence({token(TOK_LEFT_BRACKET), until_right_bracket}) //
    });
    static const inline PatternPtr stacked_expression = sequence({
        token(TOK_IDENTIFIER), // Start with a base identifier
        one_of({
            two_or_more(stackable_basic_expr),
            sequence({
                one_or_more( // Then one or more chained elements
                    one_of({
                        // Optional chained access: ?.identifier
                        sequence({token(TOK_QUESTION), token(TOK_DOT), token(TOK_IDENTIFIER)}),
                        // Optional chained grouped acess: ?.()
                        sequence({token(TOK_QUESTION), token(TOK_DOT), token(TOK_LEFT_PAREN), until_right_paren}),
                        // Optional chained array access: ?[]
                        sequence({token(TOK_QUESTION), token(TOK_LEFT_BRACKET), until_right_bracket}),
                        // Optional force-unwrap access: !.identifier
                        sequence({token(TOK_EXCLAMATION), token(TOK_DOT), token(TOK_IDENTIFIER)}),
                        // Optional force-unwrap group access: !.()
                        sequence({token(TOK_EXCLAMATION), token(TOK_DOT), token(TOK_LEFT_PAREN), until_right_paren}),
                        // Optional force-unwrap array access: ![]
                        sequence({token(TOK_EXCLAMATION), token(TOK_LEFT_BRACKET), until_right_bracket}),
                        // Variant extraction: ?(T)
                        sequence({token(TOK_QUESTION), token(TOK_LEFT_PAREN), until_right_paren}),
                        // Variant force-extract: !(T)
                        sequence({token(TOK_EXCLAMATION), token(TOK_LEFT_PAREN), until_right_paren}),
                    })),
                zero_or_more(stackable_basic_expr),
            }),
        }),
    });
    static const inline PatternPtr range_expression = balanced_match_until( //
        one_of({token(TOK_LEFT_PAREN), token(TOK_LEFT_BRACKET)}),           //
        token(TOK_RANGE),                                                   //
        one_of({token(TOK_RIGHT_PAREN), token(TOK_RIGHT_BRACKET)}),         //
        0);

    // --- STATEMENTS ---
    static const inline PatternPtr group_declaration_inferred = sequence({
        token(TOK_LEFT_PAREN), until_comma, until_right_paren, token(TOK_COLON_EQUAL) //
    });
    static const inline PatternPtr declaration_without_initializer = sequence({type, token(TOK_IDENTIFIER), token(TOK_SEMICOLON)});
    static const inline PatternPtr declaration_explicit = sequence({type, token(TOK_IDENTIFIER), token(TOK_EQUAL)});
    static const inline PatternPtr declaration_inferred = sequence({token(TOK_IDENTIFIER), token(TOK_COLON_EQUAL)});
    static const inline PatternPtr assignment = sequence({token(TOK_IDENTIFIER), token(TOK_EQUAL)});
    static const inline PatternPtr assignment_shorthand = sequence({token(TOK_IDENTIFIER), assignment_shorthand_operator});
    static const inline PatternPtr group_assignment = not_preceded_by(TOK_DOT, //
        sequence({token(TOK_LEFT_PAREN), until_right_paren, token(TOK_EQUAL)}) //
    );
    static const inline PatternPtr group_assignment_shorthand = not_preceded_by(TOK_DOT,    //
        sequence({token(TOK_LEFT_PAREN), until_right_paren, assignment_shorthand_operator}) //
    );
    static const inline PatternPtr data_field_assignment = sequence({data_access, token(TOK_EQUAL)});
    static const inline PatternPtr data_field_assignment_shorthand = sequence({data_access, assignment_shorthand_operator});
    static const inline PatternPtr grouped_data_assignment = sequence({grouped_data_access, token(TOK_EQUAL)});
    static const inline PatternPtr grouped_data_assignment_shorthand = sequence({grouped_data_access, assignment_shorthand_operator});
    static const inline PatternPtr array_assignment = sequence({array_access, token(TOK_EQUAL)});
    static const inline PatternPtr for_loop = sequence({token(TOK_FOR), until_semicolon, until_semicolon, until_colon});
    static const inline PatternPtr enhanced_for_loop = sequence({
        token(TOK_FOR),
        one_of({
            sequence({
                token(TOK_LEFT_PAREN),                                  //
                one_of({token(TOK_UNDERSCORE), token(TOK_IDENTIFIER)}), //
                token(TOK_COMMA),                                       //
                one_of({token(TOK_UNDERSCORE), token(TOK_IDENTIFIER)}), //
                token(TOK_RIGHT_PAREN)                                  //
            }),                                                         //
            token(TOK_IDENTIFIER)                                       //
        }),                                                             //
        token(TOK_IN), until_colon                                      //
    });
    static const inline PatternPtr par_for_loop = sequence({token(TOK_PARALLEL), enhanced_for_loop});
    static const inline PatternPtr while_loop = sequence({token(TOK_WHILE), until_colon});
    static const inline PatternPtr do_while_loop = sequence({token(TOK_DO), token(TOK_COLON)});
    static const inline PatternPtr if_statement = sequence({token(TOK_IF), until_colon});
    static const inline PatternPtr else_if_statement = sequence({token(TOK_ELSE), token(TOK_IF), until_colon});
    static const inline PatternPtr else_statement = sequence({token(TOK_ELSE), until_colon});
    static const inline PatternPtr return_statement = sequence({token(TOK_RETURN), until_semicolon});
    static const inline PatternPtr throw_statement = sequence({token(TOK_THROW), until_semicolon});
    static const inline PatternPtr break_statement = sequence({token(TOK_BREAK), token(TOK_SEMICOLON)});
    static const inline PatternPtr continue_statement = sequence({token(TOK_CONTINUE), token(TOK_SEMICOLON)});
    static const inline PatternPtr switch_statement = sequence({token(TOK_SWITCH), until_colon});

    // --- ERROR HANDLING ---
    static const inline PatternPtr catch_statement = sequence({
        function_call, token(TOK_CATCH), optional(token(TOK_IDENTIFIER)), token(TOK_COLON) //
    });
};
