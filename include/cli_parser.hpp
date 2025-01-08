#ifndef __CLI_PARSER_HPP__
#define __CLI_PARSER_HPP__

#include "error/error.hpp"
#include <filesystem>
#include <iostream>
#include <sys/types.h>
#include <vector>

/// CommandLineParser
///     Parses all the command line arguments and saves their values locally, accessible from outside
class CommandLineParser {
  public:
    CommandLineParser(int argc, char *argv[]) {
        // Convert the char* argv[] to a vector of strings
        args.reserve(argc - 1);
        for (size_t i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
    }

    int parse() {
        // Iterate through all command-line arguments
        std::filesystem::path cwd_path = std::filesystem::current_path();
        for (size_t i = 0; i < args.size(); ++i) {
            std::string arg = args[i];

            if (arg == "--help" || arg == "-h") {
                print_help();
                return 1;
            } else if (arg == "--file" || arg == "-f") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                source_file_path = get_absolute(cwd_path, args.at(i + 1));
                ++i;
            } else if (arg == "--out" || arg == "-o") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                out_file_path = get_absolute(cwd_path, args.at(i + 1));
                i++;
            } else if (arg == "--flags") {
                if (!n_args_follow(i + 1, "\"[flags]\"", arg)) {
                    return 1;
                }
                if (!args.at(i + 1).empty()) {
                    compile_flags = args.at(i + 1);
                    if (compile_flags.at(0) != '"' || compile_flags.at(compile_flags.length() - 1) != '"') {
                        throw_err(ERR_CLI_PARSING);
                    }
                    // Remove the " symbols of the left and right of the flags
                    compile_flags = compile_flags.substr(1, compile_flags.length() - 2);
                }
                i++;
            } else if (arg == "--output-ll-file") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                ll_file_path = get_absolute(cwd_path, args.at(i + 1));
                i++;
            } else {
                print_err("Unknown argument: " + arg);
                return 1;
            }
        }
        return 0;
    }

    std::filesystem::path source_file_path = "";
    std::filesystem::path out_file_path = "main";
    std::string compile_flags;
    std::filesystem::path ll_file_path = "";

  private:
    std::vector<std::string> args;
    std::string argument_value;

    static void print_help() {
        std::cout << "Usage: flintc [OPTIONS]\n";
        std::cout << std::endl;
        std::cout << "Available Options:\n";
        std::cout << "  --help, -h                  Show help\n";
        std::cout << "  --file, -f <file>           The file to compile\n";
        std::cout << "  --out, -o <file>            The name and path of the built output file\n";
        std::cout << "  --flags \"[flags]\"           The clang flags used to build the executable\n";
        std::cout << "  --output-ll-file <file>     Whether to output the compiled IR code.\n";
        std::cout << "                              HINT: The compiler will still compile the input file as usual.\n";
    }

    static std::filesystem::path get_absolute(const std::filesystem::path &cwd, const std::string &path) {
        std::filesystem::path file_path;
        if (path[0] == '/') {
            // Absolute path
            file_path = path;
        } else {
            // Relative path
            file_path = cwd / path;
        }
        return file_path;
    }

    static void print_err(const std::string &err) {
        std::cerr << err << "\n";
        print_help();
    }

    bool n_args_follow(const unsigned int count, const std::string &arg, const std::string &option) {
        if (args.size() <= count) {
            std::cerr << "Expected " << arg << " after '" << option << "' option!\n";
            print_help();
            return false;
        }
        return true;
    }
};

#endif
