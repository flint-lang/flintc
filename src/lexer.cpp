#include "lexer/lexer.hpp"
#include "lexer/lexer_utils.hpp"
#include "lexer/token.hpp"
#include "lexer/token_context.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "types.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

bool Lexer::file_exists_and_is_readable(const std::filesystem::path &file_path) {
    std::ifstream file(file_path);
    return file.is_open() && !file.fail();
}

std::string Lexer::load_file(const std::filesystem::path &file_path) {
    std::ifstream file(file_path);
    if (!file) {
        throw std::runtime_error("Failed to load file " + file_path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

token_list Lexer::scan() {
    tokens.clear();

    while (!is_at_end()) {
        scan_token();
    }

    // Special case for when lexing string interpolation expressions: If the token list only contains one entry, early return
    if (tokens.size() == 1) {
        return tokens;
    }

    // Remove all empty lines
    bool is_empty_line = true;
    unsigned int line_start_idx = 0;
    unsigned int current_line = 0;
    for (auto tok = tokens.begin(); tok != tokens.end();) {
        // Check if on a new line
        if (current_line != tok->line) {
            is_empty_line = true;
            line_start_idx = std::distance(tokens.begin(), tok);
            current_line = tok->line;
        }

        // Check if at the end of a line (either next token is EOL or EOF)
        bool is_line_end = (tok + 1) == tokens.end() || tok->line != (tok + 1)->line;

        if (is_line_end) {
            if (is_empty_line) {
                // Erase from line start to current position (inclusive)
                tok = tokens.erase(tokens.begin() + line_start_idx, tok + 1);
                if (tok == tokens.end()) {
                    break;
                }
                line_start_idx = std::distance(tokens.begin(), tok);
                current_line = tok->line;
            } else {
                ++tok;
            }
        } else {
            // Line is not empty if it contains a token thats _not_ INDENT
            if (tok->type != TOK_INDENT) {
                is_empty_line = false;
            }
            ++tok;
        }
    }

    return tokens;
}

std::string Lexer::to_string(const token_list &tokens) {
    std::stringstream token_stream;
    for (const TokenContext &tok : tokens) {
        token_stream << tok.lexme;
    }
    return token_stream.str();
}

void Lexer::scan_token() {
    static unsigned int space_counter = 0;
    // ensure the first character isnt skipped
    start = current;
    char character = peek();
    if (character != ' ') {
        space_counter = 0;
    }

    switch (character) {
        // single character tokens
        case '(':
            add_token(TOK_LEFT_PAREN);
            break;
        case ')':
            add_token(TOK_RIGHT_PAREN);
            break;
        case '[':
            add_token(TOK_LEFT_BRACKET);
            break;
        case ']':
            add_token(TOK_RIGHT_BRACKET);
            break;
        case '{':
            add_token(TOK_LEFT_BRACE);
            break;
        case '}':
            add_token(TOK_RIGHT_BRACE);
            break;
        case ',':
            add_token(TOK_COMMA);
            break;
        case '.':
            add_token(TOK_DOT);
            break;
        case ';':
            add_token(TOK_SEMICOLON);
            break;
        case ':':
            add_token_options(TOK_COLON, {{'=', TOK_COLON_EQUAL}, {':', TOK_REFERENCE}});
            break;
        case '?':
            add_token(TOK_QUESTION);
            break;
        case '$':
            add_token(TOK_DOLLAR);
            break;
        case '^':
            add_token(TOK_BIT_XOR);
            break;
        case '&':
            add_token(TOK_BIT_OR);
            break;
        case '_':
            if (is_alpha_num(peek_next())) {
                str();
            } else {
                add_token(TOK_UNDERSCORE);
            }
            break;
        case '#':
            add_token(TOK_FLAG);
            break;
        case '\'':
            advance(false);
            if (isascii(peek()) == 0) {
                THROW_ERR(ErrLitExpectedCharValue, ERR_LEXING, file, line, column, std::to_string(peek()));
                return;
            }
            if (peek_next() != '\'') {
                THROW_ERR(ErrLitCharLongerThanSingleCharacter, ERR_LEXING, file, line, column,
                    std::to_string(peek()) + std::to_string(peek_next()));
                return;
            }
            start = current;
            add_token(TOK_CHAR_VALUE);
        // calculational tokens
        case '+':
            add_token_options(TOK_PLUS, {{'+', TOK_INCREMENT}, {'=', TOK_PLUS_EQUALS}});
            break;
        case '-':
            add_token_options(TOK_MINUS, {{'>', TOK_ARROW}, {'-', TOK_DECREMENT}, {'=', TOK_MINUS_EQUALS}});
            break;
        case '*':
            add_token_options(TOK_MULT, {{'*', TOK_POW}, {'=', TOK_MULT_EQUALS}});
            break;
        case '/':
            if (peek_next() == '=') {
                add_token(TOK_DIV_EQUALS);
                advance();
            } else if (peek_next() == '/') {
                // traverse until the end of the line
                while (peek() != '\n' && peek_next() != '\n' && !is_at_end()) {
                    advance();
                }
                if (is_at_end()) {
                    return;
                }
            } else if (peek_next() == '*') {
                // eat the '*'
                advance();
                unsigned int line_count = 0;
                unsigned int comment_start_column = column;
                while (peek() != '*' && peek_next() != '/') {
                    if (peek() == '\n') {
                        line_count++;
                        column = 0;
                        column_diff = 0;
                    }
                    if (is_at_end()) {
                        THROW_ERR(ErrCommentUnterminatedMultiline, ERR_LEXING, file, line, comment_start_column);
                        return;
                    }
                    advance();
                }
                line += line_count;
                // eat the '/'
                advance();
                if (is_at_end()) {
                    return;
                }
            } else {
                add_token(TOK_DIV);
            }
            break;
        // relational symbols
        case '=':
            add_token_option(TOK_EQUAL, '=', TOK_EQUAL_EQUAL);
            break;
        case '<':
            add_token_options(TOK_LESS, {{'=', TOK_LESS_EQUAL}, {'<', TOK_SHIFT_LEFT}});
            break;
        case '>':
            add_token_options(TOK_GREATER, {{'=', TOK_GREATER_EQUAL}, {'>', TOK_SHIFT_RIGHT}});
            break;
        case '|':
            add_token_option(TOK_BIT_OR, '>', TOK_PIPE);
            break;
        case '!':
            if (peek_next() == '=') {
                add_token(TOK_NOT_EQUAL);
                advance();
            } else {
                std::string token_str = std::to_string(character) + std::to_string(peek_next());
                THROW_ERR(ErrUnexpectedToken, ERR_LEXING, file, line, column, token_str);
            }
            break;
        case '"':
            str();
            break;
        case '\t':
            add_token(TOK_INDENT);
            column += TAB_SIZE - 1;
            break;
        case ' ':
            space_counter++;
            if (space_counter == TAB_SIZE) {
                space_counter = 0;
                column -= TAB_SIZE;
                add_token(TOK_INDENT);
                column += TAB_SIZE;
            }
            break;
        case '\r':
            break;
        case '\n':
            add_token(TOK_EOL);
            column = 0;
            column_diff = 0;
            space_counter = 0;
            line++;
            break;
        default:
            if (is_digit(character)) {
                number();
            } else if (is_alpha(character)) {
                identifier();
            } else {
                THROW_ERR(ErrUnexpectedToken, ERR_LEXING, file, line, column, std::to_string(character));
            }
            break;
    }
    advance();
}

void Lexer::identifier() {
    // Includes all characters in the identifier which are
    // alphanumerical
    while (is_alpha_num(peek_next())) {
        advance(false);
    }

    std::string identifier = source.substr(start, current - start + 1);
    Token type = (keywords.count(identifier) > 0) ? keywords.at(identifier) : TOK_IDENTIFIER;

    add_token(type, identifier);
}

void Lexer::number() {
    while (is_digit(peek_next()) || peek_next() == '_') {
        advance(false);
    }

    if (peek_next() == '.') {
        // Get to '.'
        advance(false);
        if (!is_digit(peek_next())) {
            THROW_ERR(ErrUnexpectedTokenNumber, ERR_LEXING, file, line, column, peek_next());
            return;
        }

        while (is_digit(peek_next()) || peek_next() == '_') {
            advance(false);
        }
        add_token(TOK_FLINT_VALUE);
    } else {
        add_token(TOK_INT_VALUE);
    }
}

void Lexer::str() {
    start = current + 1;
    while ((peek_next() != '"' || (peek() == '\\' && peek_next() == '"')) && !is_at_end()) {
        if (peek() == '\n') {
            line++;
            column = 0;
            column_diff = 0;
        }
        advance(false);
    }

    if (is_at_end()) {
        THROW_ERR(ErrLitUnterminatedString, ERR_LEXING, file, line, column);
        return;
    }

    if (start == current + 1) {
        add_token(TOK_STR_VALUE, "");
    } else {
        add_token(TOK_STR_VALUE);
    }
    // eat the "
    advance();
}

char Lexer::peek() {
    if (is_at_end()) {
        return '\0'; // Not EOF, but end of string
    }
    return source.at(current);
}

char Lexer::peek_next() {
    if (static_cast<size_t>(current + 1) >= source.size()) {
        return '\0'; // Not EOF, but end of string
    }
    return source.at(current + 1);
}

bool Lexer::match(char expected) {
    return !is_at_end() && source.at(current) == expected;
}

bool Lexer::is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::is_alpha_num(char c) {
    return is_alpha(c) || is_digit(c);
}

bool Lexer::is_at_end() {
    return static_cast<size_t>(current) >= source.size();
}

char Lexer::advance(bool increment_column) {
    if (increment_column) {
        if (column_diff > 0) {
            column += column_diff;
            column_diff = 0;
        }
        column++;
    } else {
        column_diff++;
    }
    return source.at(current++);
}

void Lexer::add_token(Token token) {
    std::string lexme = source.substr(start, current - start + 1);
    if (token == TOK_FLINT_VALUE || token == TOK_INT_VALUE) {
        // Erase all '_' characters from the literal
        lexme.erase(std::remove(lexme.begin(), lexme.end(), '_'), lexme.end());
    }
    size_t pos = 0;
    while ((pos = lexme.find("\\\"", pos)) != std::string::npos) {
        lexme.replace(pos, 2, "\"");
        pos += 2;
    }
    add_token(token, lexme);
}

void Lexer::add_token(Token token, std::string lexme) {
    tokens.emplace_back(TokenContext{token, std::move(lexme), line, column});
}

void Lexer::add_token_option(Token single_token, char c, Token mult_token) {
    if (peek_next() == c) {
        std::string substr = source.substr(current, 2);
        add_token(mult_token, substr);
        advance();
    } else {
        add_token(single_token);
    }
}

void Lexer::add_token_options(Token single_token, const std::map<char, Token> &options) {
    bool token_added = false;
    for (const auto &option : options) {
        if (peek_next() == option.first) {
            add_token(option.second, source.substr(current, 2));
            advance();
            token_added = true;
            break;
        }
    }
    if (!token_added) {
        add_token(single_token);
    }
}
