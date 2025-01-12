#ifndef __CLI_PARSER_HPP__
#define __CLI_PARSER_HPP__

#include "cli_parser_base.hpp"

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
            } else if (arg == "--flags") {
                if (!n_args_follow(i + 1, "\"[flags]\"", arg)) {
                    return 1;
                }
                if (!args.at(i + 1).empty()) {
                    compile_flags.append(" ").append(args.at(i + 1));
                }
                i++;
            } else if (arg == "--output-ll-file") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                ll_file_path = get_absolute(cwd_path, args.at(i + 1));
                i++;
            } else if (arg == "--static") {
                static const std::string static_flags = "-static -Wl,--start-group -lpthread -ldl -Wl,--end-group";
                compile_flags.append(" ").append(static_flags);
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
    void print_help() override {
        std::cout << "Usage: flintc [OPTIONS]\n";
        std::cout << std::endl;
        std::cout << "Available Options:\n";
        std::cout << "  --help, -h                  Show help\n";
        std::cout << "  --file, -f <file>           The file to compile\n";
        std::cout << "  --out, -o <file>            The name and path of the built output file\n";
        std::cout << "  --flags \"[flags]\"           The clang flags used to build the executable\n";
        std::cout << "  --output-ll-file <file>     Whether to output the compiled IR code.\n";
        std::cout << "                              HINT: The compiler will still compile the input file as usual.\n";
        std::cout << "  --static                    Build the executable as static\n";
    }
};

#endif
