#pragma once

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
    TOK_EOF = -1,

    // single character tokens
    TOK_LEFT_PAREN = -2,
    TOK_RIGHT_PAREN = -3,
    TOK_LEFT_BRACKET = -4,
    TOK_RIGHT_BRACKET = -5,
    TOK_LEFT_BRACE = -6,
    TOK_RIGHT_BRACE = -7,
    TOK_COMMA = -8,
    TOK_DOT = -9,
    TOK_SEMICOLON = -10,
    TOK_COLON = -11,
    TOK_QUESTION = -12,
    TOK_UNDERSCORE = -13,
    TOK_FLAG = -14,
    TOK_DOLLAR = -15,

    // calculational tokens
    TOK_PLUS = -16,
    TOK_MINUS = -17,
    TOK_MULT = -18,
    TOK_DIV = -19,
    TOK_POW = -20,

    // assign tokens
    TOK_INCREMENT = -21,
    TOK_DECREMENT = -22,
    TOK_PLUS_EQUALS = -23,
    TOK_MINUS_EQUALS = -24,
    TOK_MULT_EQUALS = -25,
    TOK_DIV_EQUALS = -26,
    TOK_COLON_EQUAL = -27,
    TOK_EQUAL = -28,

    // relational symbols
    TOK_EQUAL_EQUAL = -29,
    TOK_NOT_EQUAL = -30,
    TOK_LESS = -31,
    TOK_LESS_EQUAL = -32,
    TOK_GREATER = -33,
    TOK_GREATER_EQUAL = -34,

    // bitwise operators
    TOK_SHIFT_LEFT = -35,
    TOK_SHIFT_RIGHT = -36,
    TOK_BIT_AND = -37,
    TOK_BIT_OR = -38,
    TOK_BIT_XOR = -39,

    // relational keywords
    TOK_AND = -40,
    TOK_OR = -41,
    TOK_NOT = -42,

    // branching keywords
    TOK_IF = -43,
    TOK_ELSE = -44,
    TOK_SWITCH = -45,

    // looping keywords
    TOK_FOR = -46,
    TOK_WHILE = -47,
    TOK_PARALLEL = -48,
    TOK_IN = -49,
    TOK_BREAK = -50,

    // function keywords
    TOK_DEF = -51,
    TOK_RETURN = -52,
    TOK_FN = -53,
    TOK_ARROW = -54,
    TOK_PIPE = -55,
    TOK_REFERENCE = -56,

    // error keywords
    TOK_ERROR = -57,
    TOK_THROW = -58,
    TOK_CATCH = -59,

    // variant keywords
    TOK_VARIANT = -60,
    TOK_ENUM = -61,

    // import keywords
    TOK_USE = -62,
    TOK_AS = -63,

    // literals
    TOK_IDENTIFIER = -64,

    // primitives
    TOK_VOID = -65,
    TOK_BOOL = -66,
    TOK_OPT = -67,
    TOK_CHAR = -68,
    TOK_STR = -69,
    TOK_FLINT = -70,
    TOK_U32 = -71,
    TOK_I32 = -72,
    TOK_I32X2 = -73,
    TOK_I32X3 = -74,
    TOK_I32X4 = -75,
    TOK_I32X8 = -76,
    TOK_U64 = -77,
    TOK_I64 = -78,
    TOK_I64X2 = -79,
    TOK_I64X3 = -80,
    TOK_I64X4 = -81,
    TOK_F32 = -82,
    TOK_F32X2 = -83,
    TOK_F32X3 = -84,
    TOK_F32X4 = -85,
    TOK_F32X8 = -86,
    TOK_F64 = -87,
    TOK_F64X2 = -88,
    TOK_F64X3 = -89,
    TOK_F64X4 = -90,

    // literals
    TOK_STR_VALUE = -91,
    TOK_INT_VALUE = -92,
    TOK_FLINT_VALUE = -93,
    TOK_CHAR_VALUE = -94,

    // builtin values
    TOK_TRUE = -95,
    TOK_FALSE = -96,
    TOK_NONE = -97,
    TOK_SOME = -98,

    // data keywords
    TOK_DATA = -99,
    TOK_SHARED = -100,
    TOK_IMMUTABLE = -101,
    TOK_ALIGNED = -102,

    // func keywords
    TOK_FUNC = -103,
    TOK_REQUIRES = -104,

    // entity keywords
    TOK_ENTITY = -105,
    TOK_EXTENDS = -106,
    TOK_LINK = -107,

    // threading keywords
    TOK_SPAWN = -108,
    TOK_SYNC = -109,
    TOK_LOCK = -110,

    // modifiers
    TOK_CONST = -111,
    TOK_MUT = -112,
    TOK_PERSISTENT = -113,

    // other tokens
    TOK_TEST = -114,
    TOK_INDENT = -115,
    TOK_EOL = -116,
};
