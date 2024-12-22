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

const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string WHITE = "\033[37m";
const std::string DEFAULT = "\033[0m";

class TestResult {
  private:
    static std::wstring convert(const std::string &str) {
        static std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        return converter.from_bytes(str);
    }

    std::string message{WHITE};
    unsigned int count = 0;

  public:
    [[nodiscard]]
    std::string get_message() const {
        return message;
    }

    [[nodiscard]]
    unsigned int get_count() const {
        return count;
    }

    void increment() {
        count++;
    }

    void add_result(TestResult &result) {
        append(result.get_message());
        count += result.get_count();
    }

    void add_result_if(TestResult &result) {
        if (result.get_count() > 0) {
            add_result(result);
        }
    }

    void append(const std::string &text) {
        message.append(text);
    }

    void append_test_name(const std::string &name, const bool is_section_header) {
        if (is_section_header) {
            append(name + "\n");
        } else {
            append(name + " ");
        }
    }

    void ok_or_not(bool was_ok) {
        if (was_ok) {
            append(GREEN + "OK" + WHITE + "\n");
        } else {
            append(RED + "FAILED" + WHITE + "\n");
        }
    }

    void print_token_stringified(const std::vector<TokenContext> &tokens) {
        append(Signature::stringify(tokens) + "\n");
    }

    void print_regex_string(const Signature::signature &signature) {
        append(Signature::get_regex_string(signature) + "\n");
    }

    void print_debug(const std::string &str) {
        append("\t" + str + "\t...");
    }
};

using test_function = std::function<TestResult()>;
using function_list = std::vector<test_function>;

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

/// run_test
///     Runs a specific test and appends its message if an error occured
static void run_test(TestResult &result, const std::function<TestResult()> &function) {
    TestResult test_result = function();
    result.add_result(test_result);
}

/// run_all_tests
///     Runs all the tests from the given tests list
static void run_all_tests(TestResult &result, const std::vector<function_list> &tests_list, bool output_all) {
    for (const function_list &tests : tests_list) {
        TestResult test_group;
        for (const test_function &test : tests) {
            run_test(test_group, test);
        }
        if (output_all) {
            result.add_result(test_group);
        } else {
            result.add_result_if(test_group);
        }
    }
}

#endif
