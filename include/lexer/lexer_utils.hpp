#pragma once

#include "token.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

static const std::unordered_map<std::string_view, Token> symbols = {
    {"", TOK_EOF},
    // single character tokens
    {"(", TOK_LEFT_PAREN},
    {")", TOK_RIGHT_PAREN},
    {"[", TOK_LEFT_BRACKET},
    {"]", TOK_RIGHT_BRACKET},
    {"{", TOK_LEFT_BRACE},
    {"}", TOK_RIGHT_BRACE},
    {",", TOK_COMMA},
    {".", TOK_DOT},
    {";", TOK_SEMICOLON},
    {":", TOK_COLON},
    {"?", TOK_QUESTION},
    {"!", TOK_EXCLAMATION},
    {"_", TOK_UNDERSCORE},
    {"#", TOK_FLAG},
    {"$", TOK_DOLLAR},
    // dual character tokens
    {"->", TOK_ARROW},
    {"|>", TOK_PIPE},
    {"::", TOK_REFERENCE},
    {"??", TOK_OPT_DEFAULT},
    {"..", TOK_RANGE},
    // calculational tokens
    {"+", TOK_PLUS},
    {"-", TOK_MINUS},
    {"*", TOK_MULT},
    {"/", TOK_DIV},
    {"%", TOK_MOD},
    {"**", TOK_POW},
    // assign tokens
    {"++", TOK_INCREMENT},
    {"--", TOK_DECREMENT},
    {"+=", TOK_PLUS_EQUALS},
    {"-=", TOK_MINUS_EQUALS},
    {"*=", TOK_MULT_EQUALS},
    {"/=", TOK_DIV_EQUALS},
    {":=", TOK_COLON_EQUAL},
    {"=", TOK_EQUAL},
    // relational symbols
    {"==", TOK_EQUAL_EQUAL},
    {"!=", TOK_NOT_EQUAL},
    {"<", TOK_LESS},
    {"<=", TOK_LESS_EQUAL},
    {">", TOK_GREATER},
    {">=", TOK_GREATER_EQUAL},
    // bitwise operators
    {"<<", TOK_SHIFT_LEFT},
    {">>", TOK_SHIFT_RIGHT},
    {"&", TOK_BIT_AND},
    {"|", TOK_BIT_OR},
    {"^", TOK_BIT_XOR},
    {"~", TOK_BIT_NEG},
    // other tokens
    {"\t", TOK_INDENT},
    {"\n", TOK_EOL},
};

static const std::unordered_map<Token, std::string_view> symbols_rev = {
    {TOK_EOF, ""},
    // single character tokens
    {TOK_LEFT_PAREN, "("},
    {TOK_RIGHT_PAREN, ")"},
    {TOK_LEFT_BRACKET, "["},
    {TOK_RIGHT_BRACKET, "]"},
    {TOK_LEFT_BRACE, "{"},
    {TOK_RIGHT_BRACE, "}"},
    {TOK_COMMA, ","},
    {TOK_DOT, "."},
    {TOK_SEMICOLON, ";"},
    {TOK_COLON, ":"},
    {TOK_QUESTION, "?"},
    {TOK_EXCLAMATION, "!"},
    {TOK_UNDERSCORE, "_"},
    {TOK_FLAG, "#"},
    {TOK_DOLLAR, "$"},
    // dual character tokens
    {TOK_ARROW, "->"},
    {TOK_PIPE, "|>"},
    {TOK_REFERENCE, "::"},
    {TOK_OPT_DEFAULT, "??"},
    {TOK_RANGE, ".."},
    // calculational tokens
    {TOK_PLUS, "+"},
    {TOK_MINUS, "-"},
    {TOK_MULT, "*"},
    {TOK_DIV, "/"},
    {TOK_MOD, "%"},
    {TOK_POW, "**"},
    // assign tokens
    {TOK_INCREMENT, "++"},
    {TOK_DECREMENT, "--"},
    {TOK_PLUS_EQUALS, "+="},
    {TOK_MINUS_EQUALS, "-="},
    {TOK_MULT_EQUALS, "*="},
    {TOK_DIV_EQUALS, "/="},
    {TOK_COLON_EQUAL, ":="},
    {TOK_EQUAL, "="},
    // relational symbols
    {TOK_EQUAL_EQUAL, "=="},
    {TOK_NOT_EQUAL, "!="},
    {TOK_LESS, "<"},
    {TOK_LESS_EQUAL, "<="},
    {TOK_GREATER, ">"},
    {TOK_GREATER_EQUAL, ">="},
    // bitwise operators
    {TOK_SHIFT_LEFT, "<<"},
    {TOK_SHIFT_RIGHT, ">>"},
    {TOK_BIT_AND, "&"},
    {TOK_BIT_OR, "|"},
    {TOK_BIT_XOR, "^"},
    {TOK_BIT_NEG, "~"},
    // other tokens
    {TOK_INDENT, "\t"},
    {TOK_EOL, "\n"},
};

static const std::unordered_map<std::string_view, Token> keywords = {
    // relational keywords
    {"and", TOK_AND},
    {"or", TOK_OR},
    {"not", TOK_NOT},
    // branching keywords
    {"if", TOK_IF},
    {"else", TOK_ELSE},
    {"switch", TOK_SWITCH},
    // looping keywords
    {"for", TOK_FOR},
    {"while", TOK_WHILE},
    {"parallel", TOK_PARALLEL},
    {"in", TOK_IN},
    {"break", TOK_BREAK},
    {"continue", TOK_CONTINUE},
    // function keywords
    {"def", TOK_DEF},
    {"return", TOK_RETURN},
    {"fn", TOK_FN},
    {"bp", TOK_BP},
    // error keywords
    {"error", TOK_ERROR},
    {"throw", TOK_THROW},
    {"catch", TOK_CATCH},
    // variant keywords
    {"variant", TOK_VARIANT},
    {"enum", TOK_ENUM},
    // import keywords
    {"use", TOK_USE},
    {"as", TOK_AS},
    // builtin values
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"none", TOK_NONE},
    // data keywords
    {"data", TOK_DATA},
    {"shared", TOK_SHARED},
    {"immutable", TOK_IMMUTABLE},
    {"aligned", TOK_ALIGNED},
    // func keywords
    {"func", TOK_FUNC},
    {"requires", TOK_REQUIRES},
    // entity keywords
    {"entity", TOK_ENTITY},
    {"extends", TOK_EXTENDS},
    {"link", TOK_LINK},
    // threading keywords
    {"spawn", TOK_SPAWN},
    {"sync", TOK_SYNC},
    {"lock", TOK_LOCK},
    // other modifiers
    {"const", TOK_CONST},
    {"mut", TOK_MUT},
    {"persistent", TOK_PERSISTENT},
    // other tokens
    {"test", TOK_TEST},
    // fip tokens
    {"extern", TOK_EXTERN},
    {"export", TOK_EXPORT},
};

static const std::unordered_map<Token, std::string_view> keywords_rev = {
    // relational keywords
    {TOK_AND, "and"},
    {TOK_OR, "or"},
    {TOK_NOT, "not"},
    // branching keywords
    {TOK_IF, "if"},
    {TOK_ELSE, "else"},
    {TOK_SWITCH, "switch"},
    // looping keywords
    {TOK_FOR, "for"},
    {TOK_WHILE, "while"},
    {TOK_PARALLEL, "parallel"},
    {TOK_IN, "in"},
    {TOK_BREAK, "break"},
    {TOK_CONTINUE, "continue"},
    // function keywords
    {TOK_DEF, "def"},
    {TOK_RETURN, "return"},
    {TOK_FN, "fn"},
    {TOK_BP, "bp"},
    // error keywords
    {TOK_ERROR, "error"},
    {TOK_THROW, "throw"},
    {TOK_CATCH, "catch"},
    // variant keywords
    {TOK_VARIANT, "variant"},
    {TOK_ENUM, "enum"},
    // import keywords
    {TOK_USE, "use"},
    {TOK_AS, "as"},
    // builtin values
    {TOK_TRUE, "true"},
    {TOK_FALSE, "false"},
    {TOK_NONE, "none"},
    // data keywords
    {TOK_DATA, "data"},
    {TOK_SHARED, "shared"},
    {TOK_IMMUTABLE, "immutable"},
    {TOK_ALIGNED, "aligned"},
    // func keywords
    {TOK_FUNC, "func"},
    {TOK_REQUIRES, "requires"},
    // entity keywords
    {TOK_ENTITY, "entity"},
    {TOK_EXTENDS, "extends"},
    {TOK_LINK, "link"},
    // threading keywords
    {TOK_SPAWN, "spawn"},
    {TOK_SYNC, "sync"},
    {TOK_LOCK, "lock"},
    // other modifiers
    {TOK_CONST, "const"},
    {TOK_MUT, "mut"},
    {TOK_PERSISTENT, "persistent"},
    // other tokens
    {TOK_TEST, "test"},
};

static const std::unordered_map<std::string_view, Token> primitives = {
    {"void", TOK_VOID},
    {"bool", TOK_BOOL},
    {"u8", TOK_U8},
    {"u8x2", TOK_U8X2},
    {"u8x3", TOK_U8X3},
    {"u8x4", TOK_U8X4},
    {"u8x8", TOK_U8X8},
    {"str", TOK_STR},
    {"flint", TOK_FLINT},
    {"u32", TOK_U32},
    {"i32", TOK_I32},
    {"bool8", TOK_BOOL8},
    {"i32x2", TOK_I32X2},
    {"i32x3", TOK_I32X3},
    {"i32x4", TOK_I32X4},
    {"i32x8", TOK_I32X8},
    {"u64", TOK_U64},
    {"i64", TOK_I64},
    {"i64x2", TOK_I64X2},
    {"i64x3", TOK_I64X3},
    {"i64x4", TOK_I64X4},
    {"f32", TOK_F32},
    {"f32x2", TOK_F32X2},
    {"f32x3", TOK_F32X3},
    {"f32x4", TOK_F32X4},
    {"f32x8", TOK_F32X8},
    {"f64", TOK_F64},
    {"f64x2", TOK_F64X2},
    {"f64x3", TOK_F64X3},
    {"f64x4", TOK_F64X4},
};

static std::string get_token_name(Token token) {
    switch (token) {
        case TOK_TYPE:
            return "TYPE";
        case TOK_STR_VALUE:
            return "STR_VALUE";
        case TOK_CHAR_VALUE:
            return "CHAR_VALUE";
        case TOK_INT_VALUE:
            return "INT_VALUE";
        case TOK_FLINT_VALUE:
            return "FLINT_VALUE";
        case TOK_INDENT:
            return "\\t";
        case TOK_EOL:
            return "\\n";
        case TOK_IDENTIFIER:
            return "IDENTIFIER";
        default:
            if (symbols_rev.find(token) != symbols_rev.end()) {
                return std::string(symbols_rev.at(token));
            } else if (keywords_rev.find(token) != keywords_rev.end()) {
                return std::string(keywords_rev.at(token));
            }
            return "UNKNOWN";
    }
}
