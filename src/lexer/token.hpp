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
    TOK_QUESTION = -12, TOK_UNDERSCORE = -13, TOK_FLAG = -14,

    // calculational tokens
    TOK_PLUS = -15, TOK_MINUS = -16, TOK_MULT = -17, TOK_DIV = -18, TOK_SQUARE = -19,

    // assign tokens
    TOK_INCREMENT = -20, TOK_DECREMENT = -21,
    TOK_PLUS_EQUALS = -22, TOK_MINUS_EQUALS = -23, TOK_MULT_EQUALS = -24, TOK_DIV_EQUALS = -25, TOK_COLON_EQUAL = -26,

    // relational symbols
    TOK_EQUAL = -27, TOK_EQUAL_EQUAL = -28, TOK_NOT_EQUAL = -29,
    TOK_LESS = -30, TOK_LESS_EQUAL = -31, TOK_GREATER = -32, TOK_GREATER_EQUAL = -33,

    // relational keywords
    TOK_AND = -34, TOK_OR = -35, TOK_NOT = -36, TOK_IS = -37, TOK_HAS = -38,
    // branching keywords
    TOK_IF = -39, TOK_ELSE = -40, TOK_SWITCH = -41,
    // looping keywords
    TOK_FOR = -42, TOK_WHILE = -43, TOK_PAR_FOR = -44, TOK_IN = -45, TOK_BREAK = -46,
    // function keywords
    TOK_DEF = -47, TOK_RETURN = -48, TOK_ARROW = -49, TOK_FN = -50, TOK_PIPE = -51, TOK_WHERE = -52,
    // error keywords
    TOK_ERROR = -53, TOK_THROW = -54, TOK_CATCH = -55, TOK_CATCH_ALL = -56,
    // variant keywords
    TOK_VARIANT = -57, TOK_ENUM = -58,
    // import keywords
    TOK_USE = -59, TOK_AS = -60, TOK_NAMESPACE = -61,

    // literals
    TOK_IDENTIFIER = -62,
    // primitives
    TOK_STR = -63, TOK_INT = -64, TOK_FLINT = -65, TOK_BOOL = -66, TOK_BYTE = -67, TOK_CHAR = -68, TOK_OPT = -69, TOK_VOID = 70,
    TOK_STR_VALUE = -71, TOK_INT_VALUE = 72, TOK_FLINT_VALUE = 73, TOK_CHAR_VALUE = -74,
    // builtin values
    TOK_TRUE = -75, TOK_FALSE = -76, TOK_NONE = -77, TOK_SOME = -78,

    // data keywords
    TOK_DATA = -79, TOK_AUTO = -80, TOK_SHARED = -81, TOK_IMMUTABLE = -82, TOK_ALIGNED = -83,
    // func keywords
    TOK_FUNC = -84, TOK_REQUIRES = -85,
    // entity keywords
    TOK_ENTITY = -86, TOK_EXTENDS = -87, TOK_LINK = -88,
    // threading keywords
    TOK_SPAWN = -89, TOK_SYNC = -90,
    // other tokens
    TOK_CONST = -91, TOK_INDENT = -92, TOK_EOL = -93,
};

#endif
