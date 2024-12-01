#ifndef __TEST_UTILS_HPP__
#define __TEST_UTILS_HPP__

#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "parser/signature.hpp"

#include <iostream>
#include <string>
#include <vector>

const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string WHITE = "\033[37m";

static void init_test() {
    std::cout << WHITE;
}

static void print_test_name(const uint indent_level, const std::string &name, const bool is_section_header) {
    for(int i = 0; i < indent_level; i++) {
        std::cout << "    ";
    }
    if(is_section_header) {
        std::cout << name << "\n";
    } else {
        std::cout << name << "...";
    }
}

static void ok_or_not(bool was_ok) {
    if(was_ok) {
        std::cout << GREEN << "OK" << WHITE << "\n";
    } else {
        std::cout << RED << "FAILED" << WHITE << "\n";
    }
}

/// create_token_vector
///     Creates a token vector from a given list of tokens
static std::vector<TokenContext> create_token_vector(const std::vector<Token> tokens) {
    std::vector<TokenContext> token_contexts;
    token_contexts.reserve(tokens.size());
    for(const Token &tok : tokens) {
        token_contexts.push_back({
            tok, "", 0
        });
    }
    return token_contexts;
}

static int run_all_tests(const std::vector<int(*)()> &tests) {
    int failed_tests = 0;
    for(auto test : tests) {
        failed_tests += test();
    }
    return failed_tests;
}

static void print_token_stringified(const std::vector<TokenContext> &tokens) {
    std::cout << Signature::stringify(tokens) << "\n";
}

static void print_regex_string(const Signature::signature &signature) {
    std::cout << Signature::get_regex_string(signature) << "\n";
}

#endif
