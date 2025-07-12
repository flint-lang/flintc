#pragma once

#include "token.hpp"

#include <cassert>
#include <memory>
#include <string>

// Forward-declaration of the type class to prevent circular imports
class Type;

/// @struct `TokenContext`
/// @brief The context for a token: where was the token (line), what is the context of the token (string) and of course, of which type was
/// the token (token). If it's a TOK_TYPE we have the `type` field too
struct TokenContext {
    Token token;
    unsigned int line;
    unsigned int column;
    union {
        std::string lexme;
        std::shared_ptr<Type> type;
    };

    // The constructor for the case the token is a regular token. In this case the token is not allowed to be `TOK_TYPE`
    TokenContext(Token t, unsigned int l, unsigned int c, const std::string &s) :
        token(t),
        line(l),
        column(c) {
        assert(token != TOK_TYPE);
        new (&lexme) std::string(s);
    }

    // The constructor for the case the token is a type token. In this case the token is only allowed to be `TOK_TYPE`
    TokenContext(Token t, unsigned int l, unsigned int c, std::shared_ptr<Type> t_ptr) :
        token(t),
        line(l),
        column(c) {
        assert(token == TOK_TYPE);
        new (&type) std::shared_ptr<Type>(t_ptr);
    }

    // The destructor to properly free the string / decrement the shared pointers arc
    ~TokenContext() {
        if (token == TOK_TYPE) {
            type.~shared_ptr();
        } else {
            lexme.~basic_string();
        }
    }

    // Copy constructor
    TokenContext(const TokenContext &other) :
        token(other.token),
        line(other.line),
        column(other.column) {
        if (token == TOK_TYPE) {
            new (&type) std::shared_ptr<Type>(other.type);
        } else {
            new (&lexme) std::string(other.lexme);
        }
    }

    // Copy assignment
    TokenContext &operator=(const TokenContext &other) {
        if (this != &other) {
            // Clean up current active member
            this->~TokenContext();

            // Copy the basic members
            token = other.token;
            line = other.line;
            column = other.column;

            // Initialize the appropriate union member
            if (token == TOK_TYPE) {
                new (&type) std::shared_ptr<Type>(other.type);
            } else {
                new (&lexme) std::string(other.lexme);
            }
        }
        return *this;
    }

    // Move constructor
    TokenContext(TokenContext &&other) noexcept :
        token(other.token),
        line(other.line),
        column(other.column) {
        if (token == TOK_TYPE) {
            new (&type) std::shared_ptr<Type>(std::move(other.type));
        } else {
            new (&lexme) std::string(std::move(other.lexme));
        }

        // Make sure other's destructor doesn't delete our moved resources
        // by setting it to a known state
        if (other.token == TOK_TYPE) {
            // Replace with a default-constructed shared_ptr
            other.type.~shared_ptr();
            new (&other.type) std::shared_ptr<Type>();
        } else {
            // Replace with an empty string
            other.lexme.~basic_string();
            new (&other.lexme) std::string();
        }
    }

    // Move assignment
    TokenContext &operator=(TokenContext &&other) noexcept {
        if (this != &other) {
            // Clean up current active member
            this->~TokenContext();

            // Copy the basic members
            token = other.token;
            line = other.line;
            column = other.column;

            // Initialize the appropriate union member by moving from other
            if (token == TOK_TYPE) {
                new (&type) std::shared_ptr<Type>(std::move(other.type));
            } else {
                new (&lexme) std::string(std::move(other.lexme));
            }

            // Reset the other object to a valid state
            if (other.token == TOK_TYPE) {
                other.type.~shared_ptr();
                new (&other.type) std::shared_ptr<Type>();
            } else {
                other.lexme.~basic_string();
                new (&other.lexme) std::string();
            }
        }
        return *this;
    }
};
