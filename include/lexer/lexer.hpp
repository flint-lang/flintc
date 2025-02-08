#ifndef __LEXER_HPP__
#define __LEXER_HPP__

#include "token.hpp"
#include "types.hpp"

#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>

class Lexer {
  public:
    explicit Lexer(const std::string &path) {
        if (!file_exists_and_is_readable(path)) {
            throw std::runtime_error("The passed file '" + path + "' could not be opened!");
        }
        file = std::filesystem::path(path).filename().string();
        source = load_file(path);
    }

    token_list scan();
    static std::string to_string(const token_list &tokens);

  public:
    static const unsigned int TAB_SIZE = 4;

  private:
    token_list tokens;
    std::string source;
    std::string file;
    int start = 0;
    int current = 0;
    unsigned int line = 1;
    unsigned int column = 1;
    unsigned int column_diff = 0;

    static bool file_exists_and_is_readable(const std::string &file_path);
    static std::string load_file(const std::string &path);

    void scan_token();

    void identifier();
    void number();
    void str();

    char peek();
    char peek_next();

    bool match(char expected);
    static bool is_alpha(char c);
    static bool is_digit(char c);
    static bool is_alpha_num(char c);
    bool is_at_end();
    char advance(bool increment_column = true);

    // Friends allow implementations to access private fields
    void add_token(Token token);
    void add_token(Token token, std::string lexme);
    void add_token_option(Token single_token, char c, Token mult_token);
    void add_token_options(Token single_token, const std::map<char, Token> &options);
};

#endif
