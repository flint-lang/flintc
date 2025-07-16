#pragma once

#include "cli_parser_base.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#ifdef DEBUG_BUILD
#include "colors.hpp"
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
            } else if (arg == "--print-libbuiltins-path") {
                std::cout << Generator::get_flintc_cache_path().string() << std::endl;
                return 1;
            } else if (arg == "--no-colors") {
                RED = "";
                GREEN = "";
                YELLOW = "";
                BLUE = "";
                WHITE = "";
                DEFAULT = "";
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
            } else if (arg == "--version") {
                print_version = true;
            } else if (arg == "--run") {
                run = true;
            } else if (arg == "--parallel") {
                parallel = true;
            } else if (arg == "--rebuild-core") {
                BUILTIN_LIBS_TO_PRINT = static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
            } else if (arg == "--target") {
                if (!n_args_follow(i + 1, "<TARGET>", arg)) {
                    return 1;
                }
                const std::string target_str = args.at(i + 1);
                if (target_str == "--help" || target_str == "-h") {
                    print_help_targets();
                    return 1;
                }
                if (target_str == "native") {
                    COMPILATION_TARGET = Target::NATIVE;
                } else if (target_str == "linux") {
                    COMPILATION_TARGET = Target::LINUX;
                } else if (target_str == "windows") {
                    COMPILATION_TARGET = Target::WINDOWS;
                } else {
                    print_err("Unknown Target: " + target_str);
                    return 1;
                }
                i++;
            } else if (starts_with(arg, "--arithmetic-")) {
                // Erase the '--arithmetic-' part of the string
                const std::string arithmetic_overflow_behaviour = arg.substr(13, arg.length() - 13);
                if (arithmetic_overflow_behaviour == "print") {
                    overflow_mode = ArithmeticOverflowMode::PRINT;
                } else if (arithmetic_overflow_behaviour == "silent") {
                    overflow_mode = ArithmeticOverflowMode::SILENT;
                } else if (arithmetic_overflow_behaviour == "crash") {
                    overflow_mode = ArithmeticOverflowMode::CRASH;
                } else if (arithmetic_overflow_behaviour == "unsafe") {
                    overflow_mode = ArithmeticOverflowMode::UNSAFE;
                } else {
                    print_err("Unknown argument: " + arg);
                    return 1;
                }
            } else if (starts_with(arg, "--array-")) {
                // Erase the '--array-' part of the string
                const std::string array_overflow_behaviour = arg.substr(8, arg.length() - 8);
                if (array_overflow_behaviour == "print") {
                    oob_mode = ArrayOutOfBoundsMode::PRINT;
                } else if (array_overflow_behaviour == "silent") {
                    oob_mode = ArrayOutOfBoundsMode::SILENT;
                } else if (array_overflow_behaviour == "crash") {
                    oob_mode = ArrayOutOfBoundsMode::CRASH;
                } else if (array_overflow_behaviour == "unsafe") {
                    oob_mode = ArrayOutOfBoundsMode::UNSAFE;
                } else {
                    print_err("Unknown argument: " + arg);
                    return 1;
                }
            } else if (starts_with(arg, "--optional-")) {
                // Erase the '--optional-' part of the string
                const std::string optional_unwrap_behaviour = arg.substr(11, arg.length() - 11);
                if (optional_unwrap_behaviour == "crash") {
                    unwrap_mode = OptionalUnwrapMode::CRASH;
                } else if (optional_unwrap_behaviour == "unsafe") {
                    unwrap_mode = OptionalUnwrapMode::UNSAFE;
                } else {
                    print_err("Unknown argument: " + arg);
                    return 1;
                }
#ifdef DEBUG_BUILD
            } else if (arg == "--no-token-stream") {
                PRINT_TOK_STREAM = false;
            } else if (arg == "--no-dep-tree") {
                PRINT_DEP_TREE = false;
            } else if (arg == "--no-ast") {
                PRINT_AST = false;
            } else if (arg == "--no-ir") {
                PRINT_IR_PROGRAM = false;
            } else if (arg == "--no-profile") {
                PRINT_PROFILE_RESULTS = false;
            } else if (arg == "--hard-crash") {
                HARD_CRASH = true;
            } else if (arg == "--no-generation") {
                NO_GENERATION = true;
            } else if (arg == "--print-ir-arithmetic") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::ARITHMETIC);
            } else if (arg == "--print-ir-array") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::ARRAY);
            } else if (arg == "--print-ir-print") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::PRINT);
            } else if (arg == "--print-ir-read") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::READ);
            } else if (arg == "--print-ir-str") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::STR);
            } else if (arg == "--print-ir-cast") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::CAST);
            } else if (arg == "--print-ir-assert") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::ASSERT);
            } else if (arg == "--print-ir-filesystem") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::FILESYSTEM);
            } else if (arg == "--print-ir-env") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::ENV);
            } else if (arg == "--print-ir-system") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::SYSTEM);
#endif
            } else {
                print_err("Unknown argument: " + arg);
                return 1;
            }
        }
        if (source_file_path.empty() && !print_version) {
            print_err("There is no file to compile!");
            return 1;
        }
        return 0;
    }

    std::filesystem::path source_file_path = "";
    std::filesystem::path out_file_path = "main";
    std::filesystem::path ll_file_path = "";
    bool build_exe{true};
    bool run{false};
    bool test{false};
    bool parallel{false};
    bool is_static{false};
    bool print_version{false};

  private:
    void print_help() override {
        std::cout << "Usage: flintc [OPTIONS]\n";
        std::cout << "\n";
        std::cout << "Available Options:\n";
        std::cout << "  --help, -h                  Show help\n";
        std::cout << "  --file, -f <file>           The file to compile\n";
        std::cout << "  --out, -o <file>            The name and path of the built output file\n";
        std::cout << "  --version                   Print the version of the compiler\n";
        // If the --test flag is set, the compiler will output a test binary. The default name "main" is overwritten to
        // "test" in that case
        std::cout << "  --test                      Output a test binary instad of the normal binary\n";
        // If the --run flag is set, the compiler will output the built binary into the .flintc directory.
        std::cout << "  --run                       Run the built binary directly without outputting it\n";
        std::cout << "  --target <TARGET>           Targets the given target platform (run --help after --target for more information)\n";
        std::cout << "  --parallel                  Compile in parallel (only recommended for bigger projects)\n";
        std::cout << "  --static                    Build the executable as static\n";
        std::cout << "  --rebuild-core              Rebuild all the core modules\n";
        std::cout << "  --print-libbuiltins-path    Prints the path to the directory where the libbuiltins.a file is saved at\n";
        std::cout << "  --no-colors                 Disables colored console output\n";
        std::cout << "  --output-ll-file <file>     Whether to output the compiled IR code\n";
        std::cout << "                              HINT: The compiler will not create an executable with this flag set";
        std::cout << std::endl;
        std::cout << "\nArithmetic Options:\n";
        std::cout << "  --arithmetic-print          [Default] Prints a small message to the console whenever an overflow occurred\n";
        std::cout << "  --arithmetic-silent         Disables the debug printing when an overflow or underflow happened\n";
        std::cout << "  --arithmetic-crash          Hard crashes when an overflow / underflow occurred\n";
        std::cout << "  --arithmetic-unsafe         Disables all overflow and underflow checks to make arithmetic operations faster";
        std::cout << std::endl;
        std::cout << "\nArray Options:\n";
        std::cout << "  --array-print               [Default] Prints a small message to the console whenever accessing an array OOB\n";
        std::cout << "  --array-silent              Disables the debug printing when OOB access happens\n";
        std::cout << "  --array-crash               Hard crashes when an OOB access happens\n";
        std::cout << "  --array-unsafe              Disables all bounds checks for array accesses\n";
        std::cout << std::endl;
        std::cout << "\nOptional Options:\n";
        std::cout << "  --optional-crash            [Default] Prints a small message and crashes whenever a bad optional unwrap happens\n";
        std::cout << "  --optional-unsafe           Disables all \"has_value\"-checks for optionals when unwrapping\n";
        std::cout << "                              HINT: All optionals which have 'none' stored on them are zero-initialized";
        std::cout << std::endl;
#ifdef DEBUG_BUILD
        std::cout << YELLOW << "\nDebug Options" << DEFAULT << ":\n";
        std::cout << "  --no-token-stream           Disables the debug printing of the lexed Token stream\n";
        std::cout << "  --no-dep-tree               Disables the debug printing of the dependency tree\n";
        std::cout << "  --no-ast                    Disables the debug printing of the parsed AST tree\n";
        std::cout << "  --no-ir                     Disables the debug printing of the generated program IR code\n";
        std::cout << "  --no-profile                Disables the debug printing of the profiling results\n";
        std::cout << "  --hard-crash                Enables the option to hard crash the program in the case of a thrown error\n";
        std::cout << "  --no-generation             Disables code generation entirely, the program exits after the parsing phase\n";
        std::cout << "                              HINT: Doesnt produce an executable";
        std::cout << std::endl;
        std::cout << YELLOW << "\nIR printing Options" << DEFAULT << ":\n";
        std::cout << "  --print-ir-arithmetic       Enables printing of the IR code for the arithmetic.o library\n";
        std::cout << "                              HINT: The arithmetic IR is not printed if '--arithmetic-unsafe' is used\n";
        std::cout << "  --print-ir-assert           Enables printing of the IR code for the assert.o library\n";
        std::cout << "  --print-ir-array            Enables printing of the IR code for the array.o library\n";
        std::cout << "  --print-ir-cast             Enables printing of the IR code for the cast.o library\n";
        std::cout << "  --print-ir-env              Enables printing of the IR code for the env.o library\n";
        std::cout << "  --print-ir-filesystem       Enables printing of the IR code for the fs.o library\n";
        std::cout << "  --print-ir-print            Enables printing of the IR code for the print.o library\n";
        std::cout << "  --print-ir-read             Enables printing of the IR code for the read.o library\n";
        std::cout << "  --print-ir-str              Enables printing of the IR code for the str.o library\n";
        std::cout << "  --print-ir-system           Enables printing of the IR code for the system.o library";
        std::cout << std::endl;
#endif
    }

    void print_help_targets() {
        std::cout << "Usage: flintc --target <TARGET>\n";
        std::cout << "\n";
        std::cout << "Available Targets:\n";
        std::cout << "  native                      [Default] The native target truple of the platform the compiler is executed on\n";
        std::cout << "  linux                       Targetting Linux (target triple 'x86_64-pc-linux-gnu')\n";
        std::cout << "  windows                     Targetting Windows (target triple 'x86_64-pc-windows-gnu')";
        std::cout << std::endl;
    }
};
