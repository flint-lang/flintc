#pragma once

#include "cli_parser_base.hpp"
#ifdef DEBUG_BUILD
#include "globals.hpp"
#endif

#include <filesystem>
#include <iostream>
#include <sys/types.h>
#include <vector>

/// CommandLineParser
///     Parses all the command line arguments and saves their values locally, accessible from outside
class CLIParserMain : public CLIParserBase {
  public:
    CLIParserMain(int argc, char *argv[]) :
        CLIParserBase(argc, argv) {}

    int parse() override {
        // Iterate through all command-line arguments
        std::filesystem::path cwd_path = std::filesystem::current_path();
        if (args.empty()) {
            print_err("No arguments were given!");
            return 1;
        }
        for (size_t i = 0; i < args.size(); ++i) {
            std::string arg = args[i];

            if (arg == "--help" || arg == "-h") {
                print_help();
                return 1;
            }
            if (arg == "--file" || arg == "-f") {
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
            } else if (starts_with(arg, "--flags=")) {
                compile_flags.append(arg.substr(8, arg.length() - 9));
            } else if (arg == "--output-ll-file") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                ll_file_path = get_absolute(cwd_path, args.at(i + 1));
                build_exe = false;
                i++;
            } else if (arg == "--static") {
                is_static = true;
                i++;
            } else if (arg == "--test") {
                test = true;
                if (out_file_path == "main") {
                    out_file_path = "test";
                }
            } else if (arg == "--run") {
                run = true;
            } else if (arg == "--parallel") {
                parallel = true;
            } else if (starts_with(arg, "--compiler=")) {
                // Erase the '--compiler=' part of the string
                compile_command = arg.substr(11, arg.length() - 11);
#ifdef DEBUG_BUILD
            } else if (arg == "--no-token-stream") {
                PRINT_TOK_STREAM = false;
            } else if (arg == "--no-dep-tree") {
                PRINT_DEP_TREE = false;
            } else if (arg == "--no-ast") {
                PRINT_AST = false;
            } else if (arg == "--no-ir") {
                PRINT_IR = false;
            } else if (arg == "--no-profile") {
                PRINT_PROFILE_RESULTS = false;
            } else if (arg == "--hard-crash") {
                HARD_CRASH = true;
#endif
            } else {
                print_err("Unknown argument: " + arg);
                return 1;
            }
        }
        if (source_file_path.empty()) {
            print_err("There is no file to compile!");
            return 1;
        }
        return 0;
    }

    std::filesystem::path source_file_path = "";
    std::filesystem::path out_file_path = "main";
    std::string compile_command{""};
    std::string compile_flags{""};
    std::filesystem::path ll_file_path = "";
    bool build_exe{true};
    bool run{false};
    bool test{false};
    bool parallel{false};
    bool is_static{false};

  private:
    void print_help() override {
        std::cout << "Usage: flintc [OPTIONS]\n";
        std::cout << "\n";
        std::cout << "Available Options:\n";
        std::cout << "  --help, -h                  Show help\n";
        std::cout << "  --file, -f <file>           The file to compile\n";
        std::cout << "  --out, -o <file>            The name and path of the built output file\n";
        // If the --test flag is set, the compiler will output a test binary. The default name "main" is overwritten to "test" in that case
        std::cout << "  --test                      Output a test binary instad of the normal binary\n";
        // If the --run flag is set, the compiler will output the built binary into the .flintc directory.
        std::cout << "  --run                       Run the built binary directly without outputting it\n";
        std::cout << "  --parallel                  Compile in parallel (only recommended for bigger projects)\n";
        std::cout << "  --flags=\"[flags]\"           The compile flags added to the external compile command\n";
        std::cout << "                              HINT: These flags have no effect when not using an external compiler\n";
        std::cout << "  --output-ll-file <file>     Whether to output the compiled IR code\n";
        std::cout << "                              HINT: The compiler will not create an executable with this flag set\n";
        std::cout << "  --static                    Build the executable as static\n";
        std::cout << "  --compiler=\"[command]\"      The external compiler command to use for code generation";
        std::cout << std::endl;
#ifdef DEBUG_BUILD
        std::cout << "\nDebug Options:\n";
        std::cout << "  --no-token-stream           Disables the debug printing of the lexed Token stream\n";
        std::cout << "  --no-dep-tree               Disables the debug printing of the dependency tree\n";
        std::cout << "  --no-ast                    Disables the debug printing of the parsed AST tree\n";
        std::cout << "  --no-ir                     Disables the debug printing of the generated IR code\n";
        std::cout << "  --no-profile                Disables the debug printing of the profiling results\n";
        std::cout << "  --hard-crash                Enables the option to hard crash the program in the case of a thrown error";
        std::cout << std::endl;
#endif
    }
};
