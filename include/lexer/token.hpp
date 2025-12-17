#pragma once

enum Token {
    TOK_EOF,

    // A token representing types (for the combinator)
    TOK_TYPE,

    // single character tokens
    TOK_LEFT_PAREN,
    TOK_RIGHT_PAREN,
    TOK_LEFT_BRACKET,
    TOK_RIGHT_BRACKET,
    TOK_LEFT_BRACE,
    TOK_RIGHT_BRACE,
    TOK_COMMA,
    TOK_DOT,
    TOK_SEMICOLON,
    TOK_COLON,
    TOK_QUESTION,
    TOK_EXCLAMATION,
    TOK_UNDERSCORE,
    TOK_ANNOTATION,
    TOK_DOLLAR,

    // dual character tokens
    TOK_ARROW,
    TOK_PIPE,
    TOK_REFERENCE,
    TOK_OPT_DEFAULT,
    TOK_RANGE,

    // arithmetic tokens
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULT,
    TOK_DIV,
    TOK_MOD,
    TOK_POW,

    // assign tokens
    TOK_INCREMENT,
    TOK_DECREMENT,
    TOK_PLUS_EQUALS,
    TOK_MINUS_EQUALS,
    TOK_MULT_EQUALS,
    TOK_DIV_EQUALS,
    TOK_COLON_EQUAL,
    TOK_EQUAL,

    // relational symbols
    TOK_EQUAL_EQUAL,
    TOK_NOT_EQUAL,
    TOK_LESS,
    TOK_LESS_EQUAL,
    TOK_GREATER,
    TOK_GREATER_EQUAL,

    // bitwise operators
    TOK_SHIFT_LEFT,
    TOK_SHIFT_RIGHT,
    TOK_BIT_AND,
    TOK_BIT_OR,
    TOK_BIT_XOR,
    TOK_BIT_NEG,

    // relational keywords
    TOK_AND,
    TOK_OR,
    TOK_NOT,

    // branching keywords
    TOK_IF,
    TOK_ELSE,
    TOK_SWITCH,

    // looping keywords
    TOK_FOR,
    TOK_DO,
    TOK_WHILE,
    TOK_PARALLEL,
    TOK_IN,
    TOK_BREAK,
    TOK_CONTINUE,

    // function keywords
    TOK_DEF,
    TOK_RETURN,
    TOK_FN,
    TOK_BP,

    // error keywords
    TOK_ERROR,
    TOK_THROW,
    TOK_CATCH,

    // variant keywords
    TOK_VARIANT,
    TOK_ENUM,

    // import keywords
    TOK_USE,
    TOK_AS,
    TOK_ALIAS,
    TOK_TYPE_KEYWORD,

    // literals
    TOK_IDENTIFIER,

    // primitives
    TOK_VOID,
    TOK_BOOL,
    TOK_U8,
    TOK_U8X2,
    TOK_U8X3,
    TOK_U8X4,
    TOK_U8X8,
    TOK_STR,
    TOK_FLINT,
    TOK_U32,
    TOK_I32,
    TOK_BOOL8,
    TOK_I32X2,
    TOK_I32X3,
    TOK_I32X4,
    TOK_I32X8,
    TOK_U64,
    TOK_I64,
    TOK_I64X2,
    TOK_I64X3,
    TOK_I64X4,
    TOK_F32,
    TOK_F32X2,
    TOK_F32X3,
    TOK_F32X4,
    TOK_F32X8,
    TOK_F64,
    TOK_F64X2,
    TOK_F64X3,
    TOK_F64X4,

    // literals
    TOK_STR_VALUE,
    TOK_INT_VALUE,
    TOK_FLINT_VALUE,
    TOK_CHAR_VALUE,

    // builtin values
    TOK_TRUE,
    TOK_FALSE,
    TOK_NONE,

    // data keywords
    TOK_DATA,
    TOK_SHARED,
    TOK_IMMUTABLE,
    TOK_ALIGNED,

    // func keywords
    TOK_FUNC,
    TOK_REQUIRES,

    // entity keywords
    TOK_ENTITY,
    TOK_EXTENDS,
    TOK_LINK,

    // threading keywords
    TOK_SPAWN,
    TOK_SYNC,
    TOK_LOCK,

    // modifiers
    TOK_CONST,
    TOK_MUT,
    TOK_PERSISTENT,

    // test keywords
    TOK_TEST,

    // fip keywords
    TOK_EXTERN,
    TOK_EXPORT,

    // other tokens
    TOK_INDENT,
    TOK_EOL,
};
