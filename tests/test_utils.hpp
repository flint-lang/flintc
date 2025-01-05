#ifndef __TEST_UTILS_HPP__
#define __TEST_UTILS_HPP__

#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "parser/signature.hpp"

#include <chrono>
#include <codecvt>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
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

/// get_command_output
///     Executes an command and returns the output of the command
static std::string get_command_output(const std::string &command) {
    constexpr std::size_t BUFFER_SIZE = 128;
    std::string output;
    FILE *pipe = popen(command.c_str(), "r");

    if (pipe == nullptr) {
        throw std::runtime_error("popen() failed!");
    }

    try {
        std::array<char, BUFFER_SIZE> buffer{};
        while (fgets(buffer.data(), BUFFER_SIZE, pipe) != nullptr) {
            output.append(buffer.data());
        }

        if (pclose(pipe) == -1) {
            throw std::runtime_error("pclose() failed!");
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }

    return output;
}

/// run_performance_test
///
static void run_performance_test(const std::string &path) {
    // Create all the paths of all strings
    std::string cwd = std::filesystem::current_path().string();
    const std::string this_path = cwd + path;
    const std::string c_bin = this_path + "/c_test";
    const std::string ft_bin = this_path + "/ft_test";
    const std::string c_compile_command = "clang " + this_path + "/test.c -o " + c_bin;
    const std::string ft_compile_command = cwd + "/build/out/flintc -f " + this_path + "/test.ft -o " + ft_bin;
    // Then, compile both the .ft and the .c file to their respective executables
    // Use the 'get_command_output' to not print any of the output to the console
    get_command_output(c_compile_command + " 2>&1");
    get_command_output(ft_compile_command + " 2>&1");
    // Finally, run both programs and save their outputs
    auto start = std::chrono::high_resolution_clock::now();
    const std::string c_test = get_command_output(c_bin);
    auto middle = std::chrono::high_resolution_clock::now();
    const std::string ft_test = get_command_output(ft_bin);
    auto end = std::chrono::high_resolution_clock::now();

    long c_duration = std::chrono::duration_cast<std::chrono::microseconds>(middle - start).count();
    long ft_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - middle).count();

    double c_duration_ms = ((double)c_duration) / 1000;
    double ft_duration_ms = ((double)ft_duration) / 1000;

    // Output the results
    std::cout << "TEST: " << this_path << std::endl;
    std::cout << "\tC  [" << std::fixed << std::setprecision(2) << c_duration_ms << " ms]: " << c_test;
    std::cout << "\tFT [" << std::fixed << std::setprecision(2) << ft_duration_ms << " ms]: " << ft_test;
}

#endif
