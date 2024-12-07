#include "../types.hpp"
#include "lexer.hpp"
#include "token.hpp"
#include "../error/error_type.hpp"
#include "../error/error.hpp"
#include "token_context.hpp"

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>

/// file_exists_and_is_readable
///     checks if the given file does exist and if it is readable or not
bool Lexer::file_exists_and_is_readable(const std::string &file_path) {
    std::ifstream file(file_path);
    return file.is_open() && !file.fail();
}

/// load_file
///     reads a file and returns the read file as a string
std::string Lexer::load_file(const std::string &file_path) {
    std::ifstream file(file_path);
    if(!file) {
        throw std::runtime_error("Failed to load file " + file_path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/// scan
///     Scans the saved source file and returns a vector
///     of all the scanned tokens.
///     If any error occurs, the scanner will cancel scanning
///     and throw an error, alongside the character(s) which
///     caused it.
token_list Lexer::scan() {
    tokens = token_list();

    while(!is_at_end()) {
        scan_token();
    }

    // Remove all empty lines (EOL tokens which are alone in a single line)
    int last_line = -1;
    for(auto tok = tokens.begin(); tok != tokens.end();) {
        if(tok->type == TOK_EOL
            && tok->line != last_line
            && (tok->line != (tok + 1)->line || (tok + 1) == tokens.end())
        ){
            tokens.erase(tok);
        } else {
            last_line = tok->line;
            tok++;
        }
    }

    return tokens;
}

/// to_string
///     Converts a token list back to its original string format
std::string Lexer::to_string(const token_list &tokens) {
    std::string token_string;

    for(const TokenContext &tok : tokens) {
        token_string += get_token_name(tok.type);
    }

    return token_string;
}

/// scan_token
///     Scans the current character and creates tokens depending on the current character
void Lexer::scan_token() {
    // ensure the first character isnt skipped
    start = current;
    char character = peek();

    switch(character) {
        // single character tokens
        case '(': add_token(TOK_LEFT_PAREN); break;
        case ')': add_token(TOK_RIGHT_PAREN); break;
        case '[': add_token(TOK_LEFT_PAREN); break;
        case ']': add_token(TOK_RIGHT_SQUARE); break;
        case '{': add_token(TOK_LEFT_BRACE); break;
        case '}': add_token(TOK_RIGHT_BRACE); break;
        case ',': add_token(TOK_COMMA); break;
        case '.': add_token(TOK_DOT); break;
        case ';': add_token(TOK_SEMICOLON); break;
        case ':': add_token_option(TOK_COLON, '=', TOK_COLON_EQUAL); break;
        case '?': add_token(TOK_QUESTION); break;
        case '_':
            if(is_alpha_num(peek_next())) {
                str();
            } else {
                add_token(TOK_UNDERSCORE);
            }
            break;
        case '#': add_token(TOK_FLAG); break;
        case '\'':
            advance();
            if(isascii(peek()) == 0) {
                throw_err(ERR_NON_CHAR_VALUE_INSIDE_CHAR);
            }
            if(peek_next() != '\'') {
                throw_err(ERR_CHAR_LONGER_THAN_SINGLE_CHARACTER);
            }
            start = current;
            add_token(TOK_CHAR_VALUE);
        // calculational tokens
        case '+':
            add_token_options(TOK_PLUS, {
                {'+', TOK_INCREMENT},
                {'=', TOK_PLUS_EQUALS}
            }); break;
        case '-':
            add_token_options(TOK_MINUS, {
                {'>', TOK_ARROW},
                {'-', TOK_DECREMENT},
                {'=', TOK_MINUS_EQUALS}
            }); break;
        case '*':
            add_token_options(TOK_MULT, {
                {'*', TOK_SQUARE},
                {'=', TOK_MULT_EQUALS}
            }); break;
        case '/':
            if(peek_next() == '=') {
               add_token(TOK_DIV_EQUALS);
            } else if (peek_next() == '/') {
                // traverse until the end of the line
                while(peek() != '\n' && !is_at_end()) {
                    advance();
                }
            } else if (peek_next() == '*') {
                // eat the '*'
                advance();
                while(peek() != '*' && peek_next() != '/') {
                    if(is_at_end()) {
                        throw_err(ERR_UNTERMINATED_MULTILINE_STRING);
                    }
                    advance();
                }
                // eat the '/'
                advance();
            } else {
                add_token(TOK_DIV);
            }
            break;
        // relational symbols
        case '=': add_token_option(TOK_EQUAL, '=', TOK_EQUAL_EQUAL); break;
        case '<': add_token_option(TOK_LESS, '=', TOK_LESS_EQUAL); break;
        case '>': add_token_option(TOK_GREATER, '=', TOK_GREATER_EQUAL); break;
        case '|':
            if(peek_next() == '>') {
                add_token(TOK_PIPE);
            } else {
                throw_err(ERR_UNEXPECTED_TOKEN);
            }
        case '"': str(); break;
        case '\t': add_token(TOK_INDENT); break;
        case ' ':
        case '\r': break;
        case '\n': add_token(TOK_EOL); line++; break;
        default:
            if(is_digit(character)) {
                number();
            } else if (is_alpha(character)) {
                identifier();
            } else {
                throw_err(ERR_UNEXPECTED_TOKEN);
            }
            break;
    }
    advance();
}

/// identifier
///     Lexes an identifier
void Lexer::identifier() {
    // Includes all characters in the identifier which are
    // alphanumerical
    while(is_alpha_num(peek_next())) {
        advance();
    }

    std::string identifier = source.substr(start, current - start + 1);
    Token type = (keywords.count(identifier) > 0)
        ? keywords.at(identifier)
        : TOK_IDENTIFIER;

    add_token(type, identifier);
}

/// number
///     Lexes a number
void Lexer::number() {
    while(is_digit(peek())) {
        advance();
    }

    if(peek() == '.' && is_digit(peek_next())) {
        // Consume the '.'
        advance();

        while(is_digit(peek())) {
            advance();
        }
        add_token(TOK_FLINT_VALUE);
    } else {
        add_token(TOK_INT_VALUE);
    }

    //addToken(NUMBER, Double.parseDouble(source.substring(start, current)));
}

/// str
///     Lexes a string value
void Lexer::str() {
    start = current + 1;
    while(peek_next() != '"' && !is_at_end()) {
        if(peek() == '\n') {
            line++;
        }
        advance();
    }

    if(is_at_end()) {
        throw_err(ERR_UNTERMINATED_STRING);
    }

    if(start == current + 1) {
        add_token(TOK_STR_VALUE, "");
    } else {
        add_token(TOK_STR_VALUE);
    }
    // eat the "
    advance();
}

/// peek
///     Peeks at the current character whithout advancing
///     the current index.
char Lexer::peek() {
    if(is_at_end()) {
        return '\0'; // Not EOF, but end of string
    }
    return source.at(current);
}

/// peek_next
///     Peeks at the next character without advancing the
///     current index.
char Lexer::peek_next() {
    if(current + 1 >= source.size()) {
        return '\0'; // Not EOF, but end of string
    }
    return source.at(current + 1);
}

/// match
///     checks if the next character aligns with the expected character
bool Lexer::match(char expected) {
    return !is_at_end() && source.at(current) == expected;
}

/// is_alpha
///     Determines whether the given character is allowed
///     to be used in identifiers('[a-zA-Z_]')
bool Lexer::is_alpha(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || c == '_';
}

/// is_digit
///     Determines whether the given character is a digit
bool Lexer::is_digit(char c) {
    return c >= '0' && c <= '9';
}

/// is_alpha_num
///     Determines whether the given character is an
///     alphanumerical character.
bool Lexer::is_alpha_num(char c) {
    return is_alpha(c) || is_digit(c);
}

/// is_at_end
///     Determines whether the scanner has reached the
///     end of the string
bool Lexer::is_at_end() {
    return current >= source.size();
}

/// advance
///     Returns the next character while also incrementing
///     the index counter for the current character.
char Lexer::advance() {
    return source.at(current++);
}

/// add_token
///     adds a given token with the added string being the
///     current character
void Lexer::add_token(Token token) {
    std::string lexme = source.substr(start, current - start + 1);
    add_token(token, lexme);
}

/// add_token
///     adds a token combined with a given string
void Lexer::add_token(Token token, std::string lexme) {
    tokens.emplace_back(TokenContext {
        token, std::move(lexme), line
    });
}

/// add_token_option
///     adds the 'mult_token' when the next character is equal
///     to 'c', otherwise adds the 'single_token'
void Lexer::add_token_option(Token single_token, char c, Token mult_token) {
    if(peek_next() == c) {
        std::string substr = source.substr(current, 2);
        add_token(mult_token, substr);
        advance();
    } else {
        add_token(single_token);
    }
}

/// add_token_options
///     adds a token depending on the next character, multiple
///     next characters are possible
void Lexer::add_token_options(Token single_token, const std::map<char, Token> &options) {
    bool token_added = false;
    for(const auto &option : options) {
        if(peek_next() == option.first) {
            add_token(option.second, source.substr(current, 2));
            advance();
            token_added = true;
            break;
        }
    }
    if(!token_added) {
        add_token(single_token);
    }
}
