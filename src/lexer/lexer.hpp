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
    {"+", TOK_PLUS},            // -15
    {"-", TOK_MINUS},           // -16
    {"*", TOK_MULT},            // -17
    {"/", TOK_DIV},             // -18
    {"**", TOK_SQUARE},         // -19
    // assign tokens
    {"++", TOK_INCREMENT},      // -20
    {"--", TOK_DECREMENT},      // -21
    {"+=", TOK_PLUS_EQUALS},    // -22
    {"-=", TOK_MINUS_EQUALS},   // -23
    {"*=", TOK_MULT_EQUALS},    // -24
    {"/=", TOK_DIV_EQUALS},     // -25
    {":=", TOK_COLON_EQUAL},    // -26
    // relational symbols
    {"=", TOK_EQUAL},           // -27
    {"==", TOK_EQUAL_EQUAL},    // -28
    {"!=", TOK_NOT_EQUAL},      // -29
    {"<", TOK_LESS},            // -30
    {"<=", TOK_LESS_EQUAL},     // -31
    {">", TOK_GREATER},         // -32
    {">=", TOK_GREATER_EQUAL},  // -33
    // function symbols
    {"->", TOK_ARROW},          // -49
    {"|>", TOK_PIPE},           // -51
    // other tokens
    {"\t", TOK_INDENT},         // -92
    {"\n", TOK_EOL}             // -93
};

static const std::map<std::string, Token> keywords = {
    //relational keywords
    {"and", TOK_AND},           // -34
    {"or", TOK_OR},             // -35
    {"not", TOK_NOT},           // -36
    {"is", TOK_IS},             // -37
    {"has", TOK_HAS},           // -38
    // branching keywords
    {"if", TOK_IF},             // -39
    {"else", TOK_ELSE},         // -40
    {"switch", TOK_SWITCH},     // -41
    // looping keywords
    {"for", TOK_FOR},           // -42
    {"while", TOK_WHILE},       // -43
    {"par_for", TOK_PAR_FOR},   // -44
    {"in", TOK_IN},             // -45
    {"break", TOK_BREAK},       // -46
    // function keywords
    {"def", TOK_DEF},           // -47
    {"return", TOK_RETURN},     // -48
    {"fn", TOK_FN},             // -50
    {"where", TOK_WHERE},       // -52
    // error keywords
    {"error", TOK_ERROR},       // -53
    {"throw", TOK_THROW},       // -54
    {"catch", TOK_CATCH},       // -55
    {"catch_all", TOK_CATCH_ALL},   // -56
    // variant keywords
    {"variant", TOK_VARIANT},   // -57
    {"enum", TOK_ENUM},         // -58
    // import keywords
    {"use", TOK_USE},           // -59
    {"as", TOK_AS},             // -60
    {"namespace", TOK_NAMESPACE}, // -61
    // primitives
    {"str", TOK_STR},           // -63
    {"int", TOK_INT},           // -64
    {"flint", TOK_FLINT},       // -65
    {"bool", TOK_BOOL},         // -66
    {"byte", TOK_BYTE},         // -67
    {"char", TOK_CHAR},         // -68
    {"Opt", TOK_OPT},           // -69
    {"void", TOK_VOID},         // -70
    // builtin values
    {"true", TOK_TRUE},         // -75
    {"false", TOK_FALSE},       // -76
    {"None", TOK_NONE},         // -77
    {"Some", TOK_SOME},         // -78
    // data keywords
    {"data", TOK_DATA},         // -79
    {"auto", TOK_AUTO},         // -80
    {"shared", TOK_SHARED},     // -81
    {"immutable", TOK_IMMUTABLE},   // -82
    {"aligned", TOK_ALIGNED},   // -83
    // func keywords
    {"func", TOK_FUNC},         // -84
    {"requires", TOK_REQUIRES}, // -85
    // entity keywords
    {"entity", TOK_ENTITY},     // -86
    {"extends", TOK_EXTENDS},   // -87
    {"link", TOK_LINK},         // -88
    // threading keywords
    {"spawn", TOK_SPAWN},       // -89
    {"sync", TOK_SYNC},         // -90
    // other modifiers
    {"const", TOK_CONST},       // -91
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
        static std::string to_string(const token_list &tokens);

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
        void add_token_options(Token single_token, const std::map<char, Token> &options);
};

#endif
