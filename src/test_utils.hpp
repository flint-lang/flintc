#ifndef __TEST_UTILS_HPP__
#define __TEST_UTILS_HPP__

#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "parser/signature.hpp"

#include <functional>
#include <string>
#include <vector>

const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string WHITE = "\033[37m";
const std::string DEFAULT = "\033[0m";

using function_list = std::vector<std::function<int()>>;
static std::string test_output_buffer;

static void init_test() {
    test_output_buffer.clear();
    test_output_buffer += WHITE;
}

static void end_test() {
    test_output_buffer += DEFAULT;
}

static void print_test_name(const unsigned int indent_level, const std::string &name, const bool is_section_header) {
    for(int i = 0; i < indent_level; i++) {
        test_output_buffer += "    ";
    }
    if(is_section_header) {
        test_output_buffer += name + "\n";
    } else {
        test_output_buffer += name + "...";
    }
}

static void ok_or_not(bool was_ok) {
    if(was_ok) {
        test_output_buffer += GREEN + "OK" + WHITE + "\n";
    } else {
        test_output_buffer += RED + "FAILED" + WHITE + "\n";
    }
}

/// create_token_vector
///     Creates a token vector from a given list of tokens
static std::vector<TokenContext> create_token_vector(const std::vector<Token> &tokens) {
    std::vector<TokenContext> token_contexts;
    token_contexts.reserve(tokens.size());
    for(const Token &tok : tokens) {
        token_contexts.push_back({
            tok, "", 0
        });
    }
    return token_contexts;
}

static int run_all_tests(const std::vector<function_list> &tests_list) {
    int failed_tests = 0;
    for(const function_list &tests : tests_list) {
        for(const std::function<int()> &test : tests) {
            failed_tests += test();
        }
    }
    return failed_tests;
}

static void print_token_stringified(const std::vector<TokenContext> &tokens) {
    test_output_buffer += Signature::stringify(tokens) + "\n";
}

static void print_regex_string(const Signature::signature &signature) {
    test_output_buffer += Signature::get_regex_string(signature) + "\n";
}

#endif
