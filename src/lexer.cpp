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
    std::ifstream file(file_path.string());
    return file.is_open() && !file.fail();
}

std::string Lexer::load_file(const std::filesystem::path &file_path) {
    std::ifstream file(file_path.string());
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
        if (!scan_token()) {
            THROW_BASIC_ERR(ERR_LEXING);
            return {};
        }
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
            is_empty_line = true;
        } else {
            // Line is not empty if it contains a token thats _not_ INDENT
            if (tok->token != TOK_INDENT) {
                is_empty_line = false;
            }
            ++tok;
        }
    }

    tokens.emplace_back(TOK_EOF, line, column, "EOF");
    total_token_count += tokens.size();
    return tokens;
}

std::string Lexer::to_string(const token_slice &tokens) {
    std::stringstream token_stream;
    std::vector<char> delimiter_stack;

    for (auto tok = tokens.first; tok != tokens.second; ++tok) {
        if (tok->token == TOK_TYPE) {
            token_stream << tok->type->to_string();
        } else if (tok->token == TOK_LESS) {
            delimiter_stack.push_back('<');
            token_stream << tok->lexme;
        } else if (tok->token == TOK_LEFT_BRACKET) {
            delimiter_stack.push_back('[');
            token_stream << tok->lexme;
        } else if (tok->token == TOK_GREATER) {
            if (!delimiter_stack.empty() && delimiter_stack.back() == '<') {
                delimiter_stack.pop_back();
            }
            token_stream << tok->lexme;
        } else if (tok->token == TOK_RIGHT_BRACKET) {
            if (!delimiter_stack.empty() && delimiter_stack.back() == '[') {
                delimiter_stack.pop_back();
            }
            token_stream << tok->lexme;
        } else if (tok->token == TOK_COMMA) {
            token_stream << tok->lexme;
            if (delimiter_stack.empty() || delimiter_stack.back() != '[') {
                token_stream << " ";
            }
        } else {
            token_stream << tok->lexme;
        }
    }

    return token_stream.str();
}

bool Lexer::scan_token() {
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
            if (peek_next() == '?') {
                add_token(TOK_OPT_DEFAULT, "??");
                advance();
            } else {
                add_token(TOK_QUESTION);
            }
            break;
        case '%':
            add_token(TOK_MOD);
            break;
        case '$':
            add_token(TOK_DOLLAR);
            break;
        case '^':
            add_token(TOK_BIT_XOR);
            break;
        case '~':
            add_token(TOK_BIT_NEG);
            break;
        case '&':
            add_token(TOK_BIT_OR);
            break;
        case '_':
            if (is_alpha_num(peek_next())) {
                if (!identifier()) {
                    THROW_BASIC_ERR(ERR_LEXING);
                    return false;
                }
            } else {
                add_token(TOK_UNDERSCORE);
            }
            break;
        case '#':
            add_token(TOK_FLAG);
            break;
        case '\'': {
            advance();
            char char_value = peek();
            if (char_value == '\\') {
                char next_val = peek_next();
                // Skip '\\'
                advance();
                switch (next_val) {
                    case 'n':
                        char_value = '\n';
                        break;
                    case 't':
                        char_value = '\t';
                        break;
                    case 'r':
                        char_value = '\r';
                        break;
                    case '\\':
                        char_value = '\\';
                        break;
                    case '0':
                        char_value = '\0';
                        break;
                    case 'x': {
                        // Hex value follows
                        // Skip 'x'
                        advance();
                        if (peek() == '\'' || peek_next() == '\'') {
                            THROW_BASIC_ERR(ERR_LEXING);
                            return false;
                        }
                        std::string hex_digits = source.substr(current, 2);
                        int hex_value = std::stoi(hex_digits, nullptr, 16);
                        char_value = static_cast<char>(hex_value);
                        // Skip one of the two hex digits
                        advance();
                        break;
                    }
                }
            }
            if (peek_next() != '\'') {
                THROW_ERR(ErrLitCharLongerThanSingleCharacter, ERR_LEXING, file, line, column,
                    std::to_string(peek()) + std::to_string(peek_next()));
                return false;
            }
            std::string char_value_str = " ";
            char_value_str[0] = char_value;
            add_token(TOK_CHAR_VALUE, char_value_str);
            // Eat the '
            advance();
            break;
        }
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
                add_token(TOK_DIV_EQUALS, "/=");
                advance();
            } else if (peek_next() == '/') {
                // traverse until the end of the line
                while (peek() != '\n' && peek_next() != '\n' && !is_at_end()) {
                    advance();
                }
                if (is_at_end()) {
                    return true;
                }
            } else if (peek_next() == '*') {
                // eat the '/'
                advance();
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
                        return false;
                    }
                    advance();
                }
                line += line_count;
                // eat the '/'
                advance();
                if (is_at_end()) {
                    return true;
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
                add_token(TOK_NOT_EQUAL, "!=");
                advance();
            } else {
                add_token(TOK_EXCLAMATION);
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
                if (!identifier()) {
                    THROW_BASIC_ERR(ERR_LEXING);
                    return false;
                }
            } else {
                THROW_ERR(ErrUnexpectedToken, ERR_LEXING, file, line, column, std::to_string(character));
                return false;
            }
            break;
    }
    advance();
    return true;
}

bool Lexer::identifier() {
    // Includes all characters in the identifier which are
    // alphanumerical
    while (is_alpha_num(peek_next())) {
        advance(false);
    }

    std::string identifier = source.substr(start, current - start + 1);
    // Check if the name starts with __flint_ as these names are not permitted for user-defined functions
    if (identifier.length() > 8 && identifier.substr(0, 8) == "__flint_") {
        THROW_ERR(ErrInvalidIdentifier, ERR_LEXING, file, line, column, identifier);
        return false;
    }
    if (primitives.count(identifier) > 0) {
        std::shared_ptr<Type> type = Type::get_primitive_type(identifier);
        tokens.emplace_back(TokenContext{TOK_TYPE, line, column, type});
        return true;
    }

    Token token = (keywords.count(identifier) > 0) ? keywords.at(identifier) : TOK_IDENTIFIER;
    add_token(token, identifier);
    return true;
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
    bool is_interpolation = !tokens.empty() && tokens.back().token == TOK_DOLLAR;
    start = current + 1;
    unsigned int depth = 1;
    bool is_interpolating = false;
    while (true) {
        if (is_interpolation) {
            const char next = peek_next();
            const char now = peek();
            if (!is_interpolating && next == '{' && now != '\\') {
                depth = 1;
                is_interpolating = true;
            } else if (!is_interpolating && next == '"' && now != '\\') {
                break;
            } else if (is_interpolating && next == '{' && now != '\\') {
                depth++;
            } else if (is_interpolating && next == '}' && now != '\\') {
                depth--;
                is_interpolating = depth != 0;
            }
        } else if ((peek_next() == '"' && peek() != '\\') || is_at_end()) {
            break;
        }
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
    tokens.emplace_back(TokenContext{token, line, column, std::move(lexme)});
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
