#ifndef __TEST_UTILS_HPP__
#define __TEST_UTILS_HPP__

#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "parser/signature.hpp"

#include <codecvt>
#include <functional>
#include <locale>
#include <string>
#include <vector>

using function_list = std::vector<std::function<int()>>;

const std::wstring RED = L"\033[31m";
const std::wstring GREEN = L"\033[32m";
const std::wstring WHITE = L"\033[37m";
const std::wstring DEFAULT = L"\033[0m";

class TestUtils {
  private:
    static std::wstring &get_buffer() {
        static std::wstring buffer;
        return buffer;
    }

    static std::wstring convert(const std::string &str) {
        static std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        return converter.from_bytes(str);
    }

    // Private constructor to prevent instantiation
    TestUtils() = delete;

  public:
    static void append(const std::wstring &text) {
        get_buffer() += text;
    }

    static void clear() {
        get_buffer().clear();
    }

    static const std::wstring &get_output() {
        return get_buffer();
    }

    static void init_test() {
        get_buffer().clear();
        append(WHITE);
    }

    static void end_test() {
        append(DEFAULT);
    }

    static void print_test_name(const std::string &name, const bool is_section_header) {
        if (is_section_header) {
            append(convert(name) + L"\n");
        } else {
            append(convert(name) + L"...");
        }
    }

    static void append_string(const std::string &str) {
        append(convert(str));
    }

    static void ok_or_not(bool was_ok) {
        if (was_ok) {
            append(GREEN + L"OK" + WHITE + L"\n");
        } else {
            append(RED + L"FAILED" + WHITE + L"\n");
        }
    }

    static void print_token_stringified(const std::vector<TokenContext> &tokens) {
        append(convert(Signature::stringify(tokens)) + L"\n");
    }

    static void print_regex_string(const Signature::signature &signature) {
        append(convert(Signature::get_regex_string(signature)) + L"\n");
    }

    static void print_debug(const std::string &str) {
        append(L"\t" + convert(str) + L"\t...");
    }
};

/// create_token_vector
///     Creates a token vector from a given list of tokens
static std::vector<TokenContext> create_token_vector(const std::vector<Token> &tokens) {
    std::vector<TokenContext> token_contexts;
    token_contexts.reserve(tokens.size());
    for (const Token &tok : tokens) {
        token_contexts.push_back({tok, "", 0});
    }
    return token_contexts;
}

/// run_all_tests
///     Runs all the tests from the given tests list
static int run_all_tests(const std::vector<function_list> &tests_list) {
    int failed_tests = 0;
    for (const function_list &tests : tests_list) {
        for (const std::function<int()> &test : tests) {
            failed_tests += test();
        }
    }
    return failed_tests;
}

#endif
