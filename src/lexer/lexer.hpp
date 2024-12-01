#ifndef __LEXER_HPP__
#define __LEXER_HPP__

#include "../types.hpp"
#include "token.hpp"

#include <map>
#include <string>
#include <array>
#include <stdexcept>
#include <filesystem>

static const std::map<std::string, Token> symbols = {
    {"", TOK_EOF},              // -1
    // single character tokens
    {"(", TOK_LEFT_PAREN},      // -2
    {")", TOK_RIGHT_PAREN},     // -3
    {"[", TOK_LEFT_SQUARE},     // -4
    {"]", TOK_RIGHT_SQUARE},    // -5
    {"{", TOK_LEFT_BRACE},      // -6
    {"}", TOK_RIGHT_BRACE},     // -7
    {",", TOK_COMMA},           // -8
    {".", TOK_DOT},             // -9
    {";", TOK_SEMICOLON},       // -10
    {":", TOK_COLON},           // -11
    {"?", TOK_QUESTION},        // -12
    {"_", TOK_UNDERSCORE},      // -13
    {"#", TOK_FLAG},            // -14
    // calculational tokens
    {"+", TOK_PLUS},            // -17
    {"-", TOK_MINUS},           // -18
    {"*", TOK_MULT},            // -19
    {"/", TOK_DIV},             // -20
    {"**", TOK_SQUARE},         // -21
    // assign tokens
    {"++", TOK_INCREMENT},      // -22
    {"--", TOK_DECREMENT},      // -23
    {"+=", TOK_PLUS_EQUALS},    // -24
    {"-=", TOK_MINUS_EQUALS},   // -25
    {"*=", TOK_MULT_EQUALS},    // -26
    {"/=", TOK_DIV_EQUALS},     // -27
    {":=", TOK_COLON_EQUAL},    // -28
    // relational symbols
    {"=", TOK_EQUAL},           // -29
    {"==", TOK_EQUAL_EQUAL},    // -30
    {"!=", TOK_NOT_EQUAL},      // -31
    {"<", TOK_LESS},            // -32
    {"<=", TOK_LESS_EQUAL},     // -33
    {">", TOK_GREATER},         // -34
    {">=", TOK_GREATER_EQUAL},  // -35
    // function symbols
    {"->", TOK_ARROW},          // -51
    {"|>", TOK_PIPE},           // -53
    // other tokens
    {"\t", TOK_INDENT},         // -88
    {"\n", TOK_EOL}             // -89
};

static const std::map<std::string, Token> keywords = {
    //relational keywords
    {"and", TOK_AND},           // -36
    {"or", TOK_OR},             // -37
    {"not", TOK_NOT},           // -38
    {"is", TOK_IS},             // -39
    {"has", TOK_HAS},           // -40
    // branching keywords
    {"if", TOK_IF},             // -41
    {"else", TOK_ELSE},         // -42
    {"switch", TOK_SWITCH},     // -43
    // looping keywords
    {"for", TOK_FOR},           // -44
    {"while", TOK_WHILE},       // -45
    {"par_for", TOK_PAR_FOR},   // -46
    {"in", TOK_IN},             // -47
    {"break", TOK_BREAK},       // -48
    // function keywords
    {"def", TOK_DEF},           // -49
    {"return", TOK_RETURN},     // -50
    {"fn", TOK_FN},             // -52
    {"where", TOK_WHERE},       // -54
    // error keywords
    {"error", TOK_ERROR},       // -55
    {"throw", TOK_THROW},       // -56
    {"catch", TOK_CATCH},       // -57
    {"catch_all", TOK_CATCH_ALL},   // -58
    // variant keywords
    {"variant", TOK_VARIANT},   // -59
    {"enum", TOK_ENUM},         // -60
    // import keywords
    {"use", TOK_USE},           // -61
    {"as", TOK_AS},             // -62
    {"namespace", TOK_NAMESPACE}, // -63
    // primitives
    {"str", TOK_STR},           // -65
    {"int", TOK_INT},           // -66
    {"flint", TOK_FLINT},       // -67
    {"bool", TOK_BOOL},         // -68
    {"byte", TOK_BYTE},         // -69
    {"char", TOK_CHAR},         // -70
    {"Opt", TOK_OPT},           // -71
    {"void", TOK_VOID},         // -72
    // builtin values
    {"true", TOK_TRUE},         // -73
    {"false", TOK_FALSE},       // -74
    {"None", TOK_NONE},         // -75
    {"Some", TOK_SOME},         // -76
    // data keywords
    {"data", TOK_DATA},         // -77
    {"auto", TOK_AUTO},         // -78
    {"shared", TOK_SHARED},     // -79
    {"immutable", TOK_IMMUTABLE},   // -80
    {"aligned", TOK_ALIGNED},   // -81
    // func keywords
    {"func", TOK_FUNC},         // -82
    {"requires", TOK_REQUIRES}, // -83
    // entity keywords
    {"entity", TOK_ENTITY},     // -84
    {"extends", TOK_EXTENDS},   // -85
    {"link", TOK_LINK},         // -86
    // other modifiers
    {"const", TOK_CONST},       // -88
};

static const std::array<std::string, 10> builtin_functions = {
    // printing
    "print",
    "printerr",
    // assertions
    "assert",
    "assert_arg",
    // concurrency
    "run_on_all",
    "map_on_all",
    "filter_on_all",
    "reduce_on_all",
    "reduce_on_pairs",
    "partition_on_all"
};

static std::string get_token_name(Token token) {
    if(token == TOK_STR_VALUE) {
        return "STR_VALUE";
    }
    if (token == TOK_CHAR_VALUE) {
        return "CHAR_VALUE";
    }
    if (token == TOK_INDENT) {
        return "\\t";
    }
    if (token == TOK_EOL) {
        return "\\n";
    }

    std::string type = "NA";
    for (const auto &kvp : symbols) {
       if (kvp.second == token) {
          type = kvp.first;
          break; // to stop searching
       }
    }
    for(const auto &kvp : keywords) {
        if (kvp.second == token) {
            type = kvp.first;
            break;
        }
    }

    if(type == "NA") {
        type = "IDENTIFIER";
    }
    return type;
}

class Lexer {
    public:
        Lexer(std::string &path) {
            if(!file_exists_and_is_readable(path)) {
                throw std::runtime_error("The passed file '" + path + "' could not be opened!");
            }
            file = std::filesystem::path(path).filename().string();
            source = load_file(path);
        }

        token_list scan();

    private:
        token_list tokens;
        std::string source;
        std::string file;
        int start = 0;
        int current = 0;
        int line = 1;

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
        char advance();

        // Friends allow implementations to access private fields
        void add_token(Token token);
        void add_token(Token token, std::string lexme);
        void add_token_option(Token single_token, char c, Token mult_token);
        void add_token_options(Token single_token, std::map<char, Token> options);
};

#endif
