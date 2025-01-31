#ifndef __TOKEN_CONTEXT_HPP__
#define __TOKEN_CONTEXT_HPP__

#include <string>

#include "token.hpp"

/// TokenContext
///     The context for a token: where was the token (line), what is the context of the token (string) and of course, of
///     which type was the token (type)
struct TokenContext {
    Token type;
    std::string lexme;
    unsigned int line;
    unsigned int column;
};

#endif
