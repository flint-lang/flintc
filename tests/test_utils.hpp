#ifndef __TEST_UTILS_HPP__
#define __TEST_UTILS_HPP__

#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "parser/signature.hpp"

#include <chrono>
#include <cmath>
#include <codecvt>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

static const std::string RED = "\033[31m";
static const std::string GREEN = "\033[32m";
static const std::string YELLOW = "\033[33m";
static const std::string BLUE = "\033[34m";
static const std::string WHITE = "\033[37m";
static const std::string DEFAULT = "\033[0m";

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
///     Runs a performance test to compare Flint to C code
static void run_performance_test(const std::filesystem::path &test_path, const std::string &compile_flags) {
    // Create all the paths of all strings
    const std::string cwd = std::filesystem::current_path().string();
    const std::string this_path = cwd / test_path;
    const std::string c_bin = this_path + "/c_test";
    const std::string ft_bin = this_path + "/ft_test";
    const std::string c_compile_command = "clang " + compile_flags + " " + this_path + "/test.c -o " + c_bin;
    const std::string ft_compile_command =
        cwd + "/build/out/flintc -f " + this_path + "/test.ft -o " + ft_bin + " --flags \"" + compile_flags + "\"";
    // Then, compile both the .ft and the .c file to their respective executables
    // Use the 'get_command_output' to not print any of the output to the console
    get_command_output(c_compile_command + " 2>&1");
    get_command_output(ft_compile_command + " 2>&1");

    // Finally, run both programs and save their outputs
    const auto start = std::chrono::high_resolution_clock::now();
    const std::string c_test = get_command_output(c_bin);
    const auto middle = std::chrono::high_resolution_clock::now();
    const std::string ft_test = get_command_output(ft_bin);
    const auto end = std::chrono::high_resolution_clock::now();

    const long c_duration = std::chrono::duration_cast<std::chrono::microseconds>(middle - start).count();
    const long ft_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - middle).count();

    const double c_duration_ms = ((double)c_duration) / 1000;
    const double ft_duration_ms = ((double)ft_duration) / 1000;

    const double perf_factor = ft_duration_ms / c_duration_ms;
    const double perf_diff_percent = -1 + perf_factor;

    std::string color;
    if (perf_diff_percent >= 0.3) {
        // FT is more than 30% slower than C
        color = RED;
    } else if (perf_diff_percent >= 0.2) {
        // FT is 20% - 30% slower than C
        color = YELLOW;
    } else if (perf_diff_percent >= 0.1) {
        // FT is 10% - 20% slower than C
        color = BLUE;
    } else if (perf_diff_percent >= 0) {
        // FT is up to 10% slower than C
        color = WHITE;
    } else {
        // FT is faster than C
        color = GREEN;
    }

    bool outputs_differ = c_test != ft_test;
    // outputs_differ = false;

    // Output the results
    std::cout << "TEST: " << test_path.string() << std::endl;
    std::cout << "\tC  [" << std::fixed << std::setprecision(2) << c_duration_ms << " ms]:        " << (outputs_differ ? c_test : "\n");
    std::cout << "\tFT [" << std::fixed << std::setprecision(2) << ft_duration_ms << " ms] [" << color << (perf_diff_percent > 0 ? "+" : "")
              << int(perf_diff_percent * 100) << "\%" << DEFAULT << "]: " << (outputs_differ ? ft_test : "\n");
}

#endif
