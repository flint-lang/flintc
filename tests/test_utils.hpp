#ifndef __TEST_UTILS_HPP__
#define __TEST_UTILS_HPP__

#include "cli_parser_base.hpp"
#include "colors.hpp"
#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "result.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using test_function = std::function<TestResult()>;
using function_list = std::vector<test_function>;

/// create_token_vector
///     Creates a token vector from a given list of tokens
static std::vector<TokenContext> create_token_vector(const std::vector<Token> &tokens) {
    std::vector<TokenContext> token_contexts;
    token_contexts.reserve(tokens.size());
    for (const Token &tok : tokens) {
        token_contexts.push_back({tok, "", 0, 0});
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

/// run_performance_test
///     Runs a performance test to compare Flint to C code
static void run_performance_test(const std::filesystem::path &test_path, const std::string &compile_flags, const unsigned int count) {
    static const std::string default_flags = "-static -Wl,--start-group -lpthread -ldl -Wl,--end-group";
    // Create all the paths of all strings
    const std::string cwd = std::filesystem::current_path().string();
    const std::string this_path = (cwd / test_path).string();
    const std::string c_bin = this_path + "/c_test";
    const std::string ft_bin = this_path + "/ft_test";
    const std::string c_compile_command = "clang " + default_flags + " " + compile_flags + " " + this_path + "/test.c -o " + c_bin;
    const std::string ft_compile_command =
        cwd + "/build/out/flintc -f " + this_path + "/test.ft -o " + ft_bin + " --flags \"" + default_flags + " " + compile_flags + "\"";
    // Delete both executables (c_test and ft_test) before compilation. This is to ensure compilation is successful
    if (std::filesystem::exists(c_bin)) {
        std::filesystem::remove(c_bin);
    }
    if (std::filesystem::exists(ft_bin)) {
        std::filesystem::remove(ft_bin);
    }
    // Then, compile both the .ft and the .c file to their respective executables
    // Use the 'get_command_output' to not print any of the output to the console
    auto [c_comp_code, c_comp_out] = CLIParserBase::get_command_output(c_compile_command + " 2>&1");
    auto [ft_comp_code, ft_comp_out] = CLIParserBase::get_command_output(ft_compile_command + " 2>&1");

    // Check if any of the compile processes failed. If yes, print the compile output:
    if (c_comp_code != 0) {
        std::cout << "\n -- C Compile command '" << YELLOW << c_compile_command << "' failed with the following output:\n"
                  << DEFAULT << c_comp_out << std::endl;
    }
    if (ft_comp_code != 0) {
        std::cout << "\n -- Flint Compile command '" << YELLOW << ft_compile_command << "' failed with the following output:\n"
                  << ft_comp_out << std::endl;
    }
    if (c_comp_code != 0 || ft_comp_code != 0) {
        return;
    }

    // Set both timers to 0
    long c_duration = 0;
    long ft_duration = 0;
    std::string c_output{};
    std::string ft_output{};
    int c_exit_code_sum = 0;
    int ft_exit_code_sum = 0;

    // Finally, run both programs and save their outputs
    for (unsigned int i = 0; i < count; ++i) {
        const auto start = std::chrono::high_resolution_clock::now();
        const auto [c_exit_code, c_test] = CLIParserBase::get_command_output(c_bin);
        const auto middle = std::chrono::high_resolution_clock::now();
        const auto [ft_exit_code, ft_test] = CLIParserBase::get_command_output(ft_bin);
        const auto end = std::chrono::high_resolution_clock::now();

        c_duration += std::chrono::duration_cast<std::chrono::microseconds>(middle - start).count();
        ft_duration += std::chrono::duration_cast<std::chrono::microseconds>(end - middle).count();
        c_output += c_test;
        ft_output += ft_test;
        c_exit_code_sum += c_exit_code;
        ft_exit_code_sum += ft_exit_code;
    }

    const double c_duration_ms = static_cast<double>(c_duration) / (1000 * count);
    const double ft_duration_ms = static_cast<double>(ft_duration) / (1000 * count);

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

    bool outputs_differ = c_output != ft_output || c_exit_code_sum != ft_exit_code_sum;
    // bool outputs_differ = false;

    // Output the results
    std::cout << "TEST: " << test_path.string() << std::endl;
    std::cout << "\tC  [" << std::fixed << std::setprecision(2) << c_duration_ms << " ms]:        " << (outputs_differ ? c_output : "")
              << "\n";
    std::cout << "\tFT [" << std::fixed << std::setprecision(2) << ft_duration_ms << " ms] [" << color << (perf_diff_percent > 0 ? "+" : "")
              << int(perf_diff_percent * 100) << "\%" << DEFAULT << "]: " << (outputs_differ ? ft_output : "") << "\n";
}

#endif
