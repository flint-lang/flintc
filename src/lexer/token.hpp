#ifndef __TOKEN_HPP__
#define __TOKEN_HPP__

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
    TOK_EOF = -1,

    // single character tokens
    TOK_LEFT_PAREN = -2, TOK_RIGHT_PAREN = -3,
    TOK_LEFT_SQUARE = -4, TOK_RIGHT_SQUARE = -5,
    TOK_LEFT_BRACE = -6, TOK_RIGHT_BRACE = -7,
    TOK_COMMA = -8, TOK_DOT = -9, TOK_SEMICOLON = -10, TOK_COLON = -11,
    TOK_QUESTION = -12, TOK_UNDERSCORE = -13, TOK_FLAG = -14, TOK_STR_VALUE = -15, TOK_CHAR_VALUE = -16,

    // calculational tokens
    TOK_PLUS = -17, TOK_MINUS = -18, TOK_MULT = -19, TOK_DIV = -20, TOK_SQUARE = -21,

    // assign tokens
    TOK_INCREMENT = -22, TOK_DECREMENT = -23,
    TOK_PLUS_EQUALS = -24, TOK_MINUS_EQUALS = -25, TOK_MULT_EQUALS = -26, TOK_DIV_EQUALS = -27, TOK_COLON_EQUAL = -28,

    // relational symbols
    TOK_EQUAL = -29, TOK_EQUAL_EQUAL = -30, TOK_NOT_EQUAL = -31,
    TOK_LESS = -32, TOK_LESS_EQUAL = -33, TOK_GREATER = -34, TOK_GREATER_EQUAL = -35,

    // relational keywords
    TOK_AND = -36, TOK_OR = -37, TOK_NOT = -38, TOK_IS = -39, TOK_HAS = -40,
    // branching keywords
    TOK_IF = -41, TOK_ELSE = -42, TOK_SWITCH = -43,
    // looping keywords
    TOK_FOR = -44, TOK_WHILE = -45, TOK_PAR_FOR = -46, TOK_IN = -47, TOK_BREAK = -48,
    // function keywords
    TOK_DEF = -49, TOK_RETURN = -50, TOK_ARROW = -51, TOK_FN = -52, TOK_PIPE = -53, TOK_WHERE = -54,
    // error keywords
    TOK_ERROR = -55, TOK_THROW = -56, TOK_CATCH = -57, TOK_CATCH_ALL = -58,
    // variant keywords
    TOK_VARIANT = -59, TOK_ENUM = -60,
    // import keywords
    TOK_USE = -61, TOK_AS = -62, TOK_NAMESPACE = -63,

    // literals
    TOK_IDENTIFIER = -64,
    // primitives
    TOK_STR = -65, TOK_INT = -66, TOK_FLINT = -67, TOK_BOOL = -68, TOK_BYTE = -69, TOK_CHAR = -70, TOK_OPT = -71, TOK_VOID = 72,
    // builtin values
    TOK_TRUE = -73, TOK_FALSE = -74, TOK_NONE = -75, TOK_SOME = -76,

    // data keywords
    TOK_DATA = -77, TOK_AUTO = -78, TOK_SHARED = -79, TOK_IMMUTABLE = -80, TOK_ALIGNED = -81,
    // func keywords
    TOK_FUNC = -82, TOK_REQUIRES = -83,
    // entity keywords
    TOK_ENTITY = -84, TOK_EXTENDS = -85, TOK_LINK = -86,
    // other tokens
    TOK_CONST = -87, TOK_INDENT = -88, TOK_EOL = -89,
};

#endif
