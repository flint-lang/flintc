#pragma once

#include "resolver/resolver.hpp"
#include "token.hpp"
#include "types.hpp"

#include <atomic>
#include <map>
#include <string>

/// @class `Lexer`
/// @brief This class is responsible for lexing a character stream and outputting a token stream
class Lexer {
  public:
    explicit Lexer(const std::filesystem::path &file_path, const std::string &file_content) :
        source(file_content),
        file_hash(Hash(file_path.empty() ? file_path : std::filesystem::absolute(file_path))) {
        if (file_hash.empty()) {
            file_id = UINT32_MAX;
            return;
        }
        auto it = std::find(Resolver::file_ids.begin(), Resolver::file_ids.end(), file_hash);
        if (it == Resolver::file_ids.end()) {
            Resolver::file_ids.push_back(file_hash);
            file_id = Resolver::file_ids.size() - 1;
        } else {
            file_id = static_cast<unsigned int>(std::distance(Resolver::file_ids.begin(), it));
        }
    }

    /// @function `scan`
    /// @brief Scans the given file of the lexer and returns the token stream
    ///
    /// @return `token_list` The list of all tokens from the lexed file
    token_list scan();

    /// @function `to_string`
    /// @brief Converts a stream of tokens back to a stream of characters
    ///
    /// @param `tokens` The list of tokens to "convert"
    /// @return `std::string` The stream of characters from the given list of tokens
    static std::string to_string(const token_slice &tokens);

    /// @function `is_alpha`
    /// @brief Determines whether the given character is allowed to be used in identifiers('[a-zA-Z_]')
    ///
    /// @param `c` The character to check
    /// @return `bool` Whether the given character is alpha
    [[nodiscard]] static bool is_alpha(char c);

    /// @function `is_digit`
    /// @brief Determines whther the given character is a digit
    ///
    /// @param `c` The character to check
    /// @return `bool` Whether the character is a digit
    [[nodiscard]] static inline bool is_digit(char c);

    /// @function `is_alpha_num`
    /// @brief Determines whether the given character is alpha or a number
    ///
    /// @param `c` The character to check
    /// @return `bool` Whetehr the given character is alpha or a number
    [[nodiscard]] static bool is_alpha_num(char c);

  public:
    /// @var `lines`
    /// @brief A list of all the lines of the file where each line is a slice into the file + the indentation level of that line
    std::vector<std::pair<unsigned int, std::string_view>> lines;

    /// @var `TAB_SIZE`
    /// @brief This variable determines how many spaces are equal to one tab
    ///
    /// @details This variable is used to correctly set the column for indents and to interpret multiple spaces as tabs
    static constexpr unsigned int TAB_SIZE = 4;

    /// @var `total_token_count`
    /// @brief This variable tracks how many tokens have been lexed across all files
    static inline std::atomic_uint total_token_count;

    /// @var `total_lexing_time_ns`
    /// @brief This variable tracks the total time spent lexing across all files (in nanoseconds)
    static inline std::atomic_uint64_t total_lexing_time_ns;

  private:
    /// @var `tokens`
    /// @brief The list of all lexed tokens so far
    token_list tokens;

    /// @var `source`
    /// @brief The source file's content which will be lexed to a token stream
    const std::string &source;

    /// @var `file_hash`
    /// @brief The hash of the source file which is currently being tokenized
    Hash file_hash;

    /// @var `file_id`
    /// @brief The ID of the source file which is currently being tokenized
    unsigned int file_id;

    /// @var `start`
    /// @brief This variable is used for extracting longer strings or other values from the file
    int start = 0;

    /// @var `current`
    /// @brief The current character the lexer is at in the source file
    int current = 0;

    /// @var `line`
    /// @brief The current line of the source file
    unsigned int line = 1;

    /// @var `column`
    /// @brief The current column of the source file
    unsigned int column = 1;

    /// @var `column_diff`
    /// @brief This variable is used to defer the increasing of the column
    unsigned int column_diff = 0;

    /// @var `line_vars`
    /// @brief A collection of all variables responsible for creating the source `line` vector
    struct {
        /// @var `offset`
        /// @brief The offset within the current line
        unsigned int offset = 1;

        /// @var `indent_lvl`
        /// @brief The indentation level of the current line
        unsigned int indent_lvl = 0;

        /// @var `is_at_start`
        /// @brief Whether the line is at it's beginning
        bool is_at_start = true;
    } line_vars;

    /// @function `scan_token`
    /// @brief Scans the current character and creates tokens depending on the current character
    ///
    /// @return `bool` Whether scanning for the next token was successful
    [[nodiscard]] bool scan_token();

    /// @function `identifier`
    /// @brief Lexes an identifier
    ///
    /// @return `bool` Whether lexing the identifier was successful (It fails when it starts with __flint_)
    [[nodiscard]] bool identifier();

    /// @function `number`
    /// @brief Lexes a number
    ///
    /// @return `bool` Whether lexing the number was successful
    [[nodiscard]] bool number();

    /// @function `str`
    /// @brief Lexes a string value
    void str();

    /// @function `peek`
    /// @brief Peeks at the current character whithout advancing the current index
    ///
    /// @return `char` The character we are currently at
    [[nodiscard]] char peek();

    /// @function `peek_next`
    /// @brief Peeks at the next character without advancing the current index
    ///
    /// @return `char` The next character
    [[nodiscard]] char peek_next();

    /// @function `match`
    /// @brief Checks if the next character aligns with the expected character
    ///
    /// @param `expected` The expected character
    /// @return `bool` Whether the next character matches the expected character
    [[nodiscard]] bool match(char expected);

    /// @function `is_at_end`
    /// @brief Determines whether the scanner has reached the end of the file string
    ///
    /// @return `bool` Determines whether the scanner has reached the end of the file string
    [[nodiscard]] bool is_at_end();

    /// @function `advance`
    /// @brief Returns the next character while also incrementing the index counter for the current character
    ///
    /// @param `increment_column` Whether to increment the current column, for example when lexing str's
    /// @return `char` The next character
    char advance(bool increment_column = true);

    /// @function `add_token`
    /// @brief adds a given token with the added string being the current character
    ///
    /// @param `token` The token type to add to the list
    void add_token(Token token);

    /// @function `add_token`
    /// @brief Adds a token combined with a given string
    ///
    /// @param `token` The token type to add
    /// @param `lexme` The string of the token to add
    void add_token(Token token, std::string_view lexme);

    /// @function `add_token_option`
    /// @brief Adds the 'mult_token' when the next character is equal to 'c', otherwise adds the 'single_token'
    ///
    /// @param `single_token` The token type to add if the next character is not `c`
    /// @param `c` The next character of the multi-character token (for example `->`)
    /// @param `mult_token` The token type to add if the next character is `c`
    void add_token_option(Token single_token, char c, Token mult_token);

    /// @function `add_token_options`
    /// @brief Adds a token depending on the next character, multiple next characters are possible
    ///
    /// @param `single_token` The token type to add if none of the characters in the options map match
    /// @param `options` A map of token types and their respective characters
    void add_token_options(Token single_token, const std::map<char, Token> &options);
};
