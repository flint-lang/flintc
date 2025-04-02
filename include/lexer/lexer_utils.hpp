#pragma once

#include "token.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

static const std::unordered_map<std::string_view, Token> symbols = {
    {"", TOK_EOF}, // -1
    // single character tokens
    {"(", TOK_LEFT_PAREN},   // -2
    {")", TOK_RIGHT_PAREN},  // -3
    {"[", TOK_LEFT_SQUARE},  // -4
    {"]", TOK_RIGHT_SQUARE}, // -5
    {"{", TOK_LEFT_BRACE},   // -6
    {"}", TOK_RIGHT_BRACE},  // -7
    {",", TOK_COMMA},        // -8
    {".", TOK_DOT},          // -9
    {";", TOK_SEMICOLON},    // -10
    {":", TOK_COLON},        // -11
    {"?", TOK_QUESTION},     // -12
    {"_", TOK_UNDERSCORE},   // -13
    {"#", TOK_FLAG},         // -14
    {"$", TOK_DOLLAR},       // -15
    // calculational tokens
    {"+", TOK_PLUS},  // -16
    {"-", TOK_MINUS}, // -17
    {"*", TOK_MULT},  // -18
    {"/", TOK_DIV},   // -19
    {"**", TOK_POW},  // -20
    // assign tokens
    {"++", TOK_INCREMENT},    // -21
    {"--", TOK_DECREMENT},    // -22
    {"+=", TOK_PLUS_EQUALS},  // -23
    {"-=", TOK_MINUS_EQUALS}, // -24
    {"*=", TOK_MULT_EQUALS},  // -25
    {"/=", TOK_DIV_EQUALS},   // -26
    {":=", TOK_COLON_EQUAL},  // -27
    {"=", TOK_EQUAL},         // -28
    // relational symbols
    {"==", TOK_EQUAL_EQUAL},   // -29
    {"!=", TOK_NOT_EQUAL},     // -30
    {"<", TOK_LESS},           // -31
    {"<=", TOK_LESS_EQUAL},    // -32
    {">", TOK_GREATER},        // -33
    {">=", TOK_GREATER_EQUAL}, // -34
    // bitwise operators
    {"<<", TOK_SHIFT_LEFT},  // -35
    {">>", TOK_SHIFT_RIGHT}, // -36
    {"&", TOK_BIT_AND},      // -37
    {"|", TOK_BIT_OR},       // -38
    {"^", TOK_BIT_XOR},      // -39
    // function symbols
    {"->", TOK_ARROW},     // -54
    {"|>", TOK_PIPE},      // -55
    {"::", TOK_REFERENCE}, // -56
    // other tokens
    {"\t", TOK_INDENT}, // -115
    {"\n", TOK_EOL}     // -116
};

static const std::unordered_map<std::string_view, Token> keywords = {
    // relational keywords
    {"and", TOK_AND}, // -40
    {"or", TOK_OR},   // -41
    {"not", TOK_NOT}, // -42
    // branching keywords
    {"if", TOK_IF},         // -43
    {"else", TOK_ELSE},     // -44
    {"switch", TOK_SWITCH}, // -45
    // looping keywords
    {"for", TOK_FOR},           // -46
    {"while", TOK_WHILE},       // -47
    {"parallel", TOK_PARALLEL}, // -48
    {"in", TOK_IN},             // -49
    {"break", TOK_BREAK},       // -50
    // function keywords
    {"def", TOK_DEF},       // -51
    {"return", TOK_RETURN}, // -52
    {"fn", TOK_FN},         // -53
    // error keywords
    {"error", TOK_ERROR}, // -57
    {"throw", TOK_THROW}, // -58
    {"catch", TOK_CATCH}, // -59
    // variant keywords
    {"variant", TOK_VARIANT}, // -60
    {"enum", TOK_ENUM},       // -61
    // import keywords
    {"use", TOK_USE}, // -62
    {"as", TOK_AS},   // -63
    // primitives
    {"void", TOK_VOID},   // -65
    {"bool", TOK_BOOL},   // -66
    {"Opt", TOK_OPT},     // -67
    {"char", TOK_CHAR},   // -68
    {"str", TOK_STR},     // -69
    {"flint", TOK_FLINT}, // -70
    {"u32", TOK_U32},     // -71
    {"i32", TOK_I32},     // -72
    {"i32x2", TOK_I32X2}, // -73
    {"i32x3", TOK_I32X3}, // -74
    {"i32x4", TOK_I32X4}, // -75
    {"i32x8", TOK_I32X8}, // -76
    {"u64", TOK_U64},     // -77
    {"i64", TOK_I64},     // -78
    {"i64x2", TOK_I64X2}, // -79
    {"i64x3", TOK_I64X3}, // -80
    {"i64x4", TOK_I64X4}, // -81
    {"f32", TOK_F32},     // -82
    {"f32x2", TOK_F32X2}, // -83
    {"f32x3", TOK_F32X3}, // -84
    {"f32x4", TOK_F32X4}, // -85
    {"f32x8", TOK_F32X8}, // -86
    {"f64", TOK_F64},     // -87
    {"f64x2", TOK_F64X2}, // -88
    {"f64x3", TOK_F64X3}, // -89
    {"f64x4", TOK_F64X4}, // -90
    // builtin values
    {"true", TOK_TRUE},   // -95
    {"false", TOK_FALSE}, // -96
    {"None", TOK_NONE},   // -97
    {"Some", TOK_SOME},   // -98
    // data keywords
    {"data", TOK_DATA},           // -99
    {"shared", TOK_SHARED},       // -102
    {"immutable", TOK_IMMUTABLE}, // -103
    {"aligned", TOK_ALIGNED},     // -104
    // func keywords
    {"func", TOK_FUNC},         // -103
    {"requires", TOK_REQUIRES}, // -104
    // entity keywords
    {"entity", TOK_ENTITY},   // -105
    {"extends", TOK_EXTENDS}, // -106
    {"link", TOK_LINK},       // -107
    // threading keywords
    {"spawn", TOK_SPAWN}, // -108
    {"sync", TOK_SYNC},   // -109
    {"lock", TOK_LOCK},   // -110
    // other modifiers
    {"const", TOK_CONST},           // -111
    {"mut", TOK_MUT},               // -112
    {"persistent", TOK_PERSISTENT}, // -113
    // other tokens
    {"test", TOK_TEST}, // -114
};

static std::string get_token_name(Token token) {
    if (token == TOK_STR_VALUE) {
        return "STR_VALUE";
    }
    if (token == TOK_CHAR_VALUE) {
        return "CHAR_VALUE";
    }
    if (token == TOK_INT_VALUE) {
        return "INT_VALUE";
    }
    if (token == TOK_FLINT_VALUE) {
        return "FLINT_VALUE";
    }
    if (token == TOK_INDENT) {
        return "\\t";
    }
    if (token == TOK_EOL) {
        return "\\n";
    }

    std::string type = "IDENTIFIER";
    for (const auto &kvp : symbols) {
        if (kvp.second == token) {
            type = kvp.first;
            break; // to stop searching
        }
    }
    for (const auto &kvp : keywords) {
        if (kvp.second == token) {
            type = kvp.first;
            break;
        }
    }
    return type;
}
