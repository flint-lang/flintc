#pragma once

#include "cli_parser_base.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <vector>

/// CommandLineParser
///     Parses all the command line arguments and saves their values locally, accessible from outside
class CLIParserTests : public CLIParserBase {
  public:
    CLIParserTests(int argc, char *argv[]) :
        CLIParserBase(argc, argv) {}

    int parse() override {
        // Iterate through all command-line arguments
        std::filesystem::path cwd_path = std::filesystem::current_path();
        for (size_t i = 0; i < args.size(); ++i) {
            std::string arg = args.at(i);

            if (arg == "--help" || arg == "-h") {
                print_help();
                return 1;
            }
            if (arg == "--count" || arg == "-c") {
                if (!n_args_follow(i + 1, "<num>", arg)) {
                    return 1;
                }
                if (!is_integer(args.at(i + 1))) {
                    print_err("Count must be an integer");
                    return 1;
                }
                int num = std::stoi(args.at(i + 1));
                if (num < 1) {
                    std::cout << "Warning: Count is less than 1, setting it to 1\n";
                    num = 1;
                }
                count = static_cast<unsigned int>(num);
                ++i;
            } else if (arg == "--flags") {
                if (!n_args_follow(i + 1, "\"[flags]\"", arg)) {
                    return 1;
                }
                if (!args.at(i + 1).empty()) {
                    compile_flags = args.at(i + 1);
                }
                i++;
            } else if (arg == "--test-performance" || arg == "-p") {
                test_performance = true;
            } else if (arg == "--no-unit-tests" || arg == "-n") {
                unit_tests = false;
            } else if (arg == "--fuzzy" || arg == "-f") {
                fuzzy_testing = true;
                if (i + 1 < args.size()) {
                    // Try to parse the next argument as a number, if it fails we dont have a "real" number as the next argument
                    try {
                        fuzzy_count = std::stoul(args.at(i + 1));
                        ++i;
                    } catch ([[maybe_unused]] const std::invalid_argument &) {
                        // Not a number, just continue
                    } catch ([[maybe_unused]] const std::out_of_range &) {
                        // Number too large, just continue
                    }
                }
            } else {
                print_err("Unknown argument: " + arg);
                return 1;
            }
        }
        return 0;
    }

    std::string compile_flags;
    unsigned int count{1};
    bool unit_tests{true};
    bool test_performance{false};
    bool fuzzy_testing{false};
    unsigned long fuzzy_count = 1000000;

  private:
    void print_help() override {
        std::cout << "Usage: tests [OPTIONS]\n";
        std::cout << std::endl;
        std::cout << "Available Options:\n";
        std::cout << "  --help, -h                  Show help\n";
        std::cout << "  --no-unit-tests, -n         Disables unit testing (not recommended)\n";
        std::cout << "  --test-performance, -p      Run all performance tests\n";
        std::cout << "  --fuzzy, -f [<num>]         Run all fuzzy tests <num> times (default = 1.000.000)\n";
        std::cout << "\n";
        std::cout << "Performance Test Options:\n";
        std::cout << "  --count, -c <num>           The count how often each test will run. (default = 1)\n";
        std::cout << "                              The end result will be the mean of all results.\n";
        std::cout << "  --flags \"[flags]\"           The clang flags used to build the executables (Both C and Flint)\n";
    }

    static bool is_integer(const std::string &str) {
        std::istringstream iss(str);
        int num;
        iss >> num;
        // Check if the conversion succeeded and if the end was reached
        return !iss.fail() && iss.eof();
    }
};
