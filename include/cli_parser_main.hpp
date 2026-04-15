#pragma once

#include "cli_parser_base.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#ifdef DEBUG_BUILD
#include "colors.hpp"
#endif

#include <filesystem>
#include <iostream>
#include <ostream>
#include <sstream>
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

            if (arg.length() >= 2 && arg[0] == '-' && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); j++) {
                    switch (arg[j]) {
                        case 'h':
                            print_help();
                            std::exit(0);
                        case 'f':
                            if (j + 1 < arg.length()) {
                                std::cerr << "Expected the 'f' to be the last single-element argument in the argument '" << arg << "'!\n";
                                return 1;
                            }
                            if (!n_args_follow(i + 1, "<file>", arg)) {
                                return 1;
                            }
                            source_file_path = get_absolute(cwd_path, args.at(i + 1));
                            ++i;
                            break;
                        case 'o':
                            if (j + 1 < arg.length()) {
                                std::cerr << "Expected the 'o' to be the last single-element argument in the argument '" << arg << "'!\n";
                                return 1;
                            }
                            if (!n_args_follow(i + 1, "<file>", arg)) {
                                return 1;
                            }
                            out_file_path = get_absolute(cwd_path, args.at(i + 1));
                            i++;
                            break;
                        case 'r':
                            run = true;
                            break;
                        case 'p':
                            parallel = true;
                            break;
                        case 's':
                            is_static = true;
                            break;
                        case 't':
                            test = true;
                            if (out_file_path == "main") {
                                out_file_path = "test";
                            }
                            break;
                    }
                }
                continue;
            }
            if (arg == "--help") {
                print_help();
                std::exit(0);
            } else if (arg == "--file") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                source_file_path = get_absolute(cwd_path, args.at(i + 1));
                ++i;
            } else if (arg == "--out") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                out_file_path = get_absolute(cwd_path, args.at(i + 1));
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
            } else if (arg == "--static") {
                is_static = true;
                i++;
            } else if (arg == "--version") {
                print_version = true;
            } else if (arg == "--target") {
                if (!n_args_follow(i + 1, "<TARGET>", arg)) {
                    return 1;
                }
                const std::string target_str = args.at(i + 1);
                if (target_str == "--help" || target_str == "-h") {
                    print_help_targets();
                    std::exit(0);
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
            } else if (arg == "--arithmetic") {
                if (!n_args_follow(i + 1, "<MODE>", arg)) {
                    return 1;
                }
                const std::string arithmetic_str = args.at(i + 1);
                if (arithmetic_str == "--help" || arithmetic_str == "-h") {
                    print_help_arithmetic();
                    std::exit(0);
                }
                if (arithmetic_str == "print") {
                    overflow_mode = ArithmeticOverflowMode::PRINT;
                } else if (arithmetic_str == "silent") {
                    overflow_mode = ArithmeticOverflowMode::SILENT;
                } else if (arithmetic_str == "crash") {
                    overflow_mode = ArithmeticOverflowMode::CRASH;
                } else if (arithmetic_str == "unsafe") {
                    overflow_mode = ArithmeticOverflowMode::UNSAFE;
                } else {
                    print_err("Unknown Mode: " + arithmetic_str);
                    return 1;
                }
                i++;
            } else if (arg == "--array") {
                if (!n_args_follow(i + 1, "<MODE>", arg)) {
                    return 1;
                }
                const std::string array_str = args.at(i + 1);
                if (array_str == "--help" || array_str == "-h") {
                    print_help_array();
                    std::exit(0);
                }
                if (array_str == "print") {
                    oob_mode = ArrayOutOfBoundsMode::PRINT;
                } else if (array_str == "silent") {
                    oob_mode = ArrayOutOfBoundsMode::SILENT;
                } else if (array_str == "crash") {
                    oob_mode = ArrayOutOfBoundsMode::CRASH;
                } else if (array_str == "unsafe") {
                    oob_mode = ArrayOutOfBoundsMode::UNSAFE;
                } else {
                    print_err("Unknown Mode: " + array_str);
                    return 1;
                }
                i++;
            } else if (arg == "--opaque") {
                if (!n_args_follow(i + 1, "<MODE>", arg)) {
                    return 1;
                }
                const std::string opaque_str = args.at(i + 1);
                if (opaque_str == "--help" || opaque_str == "-h") {
                    print_help_opaque();
                    std::exit(0);
                }
                if (opaque_str == "print") {
                    opaque_leak_mode = OpaqueLeakMode::PRINT;
                } else if (opaque_str == "silent") {
                    opaque_leak_mode = OpaqueLeakMode::SILENT;
                } else if (opaque_str == "crash") {
                    opaque_leak_mode = OpaqueLeakMode::CRASH;
                } else {
                    print_err("Unknown Mode: " + opaque_str);
                    return 1;
                }
                i++;
            } else if (arg == "--optional") {
                if (!n_args_follow(i + 1, "<MODE>", arg)) {
                    return 1;
                }
                const std::string optional_str = args.at(i + 1);
                if (optional_str == "--help" || optional_str == "-h") {
                    print_help_optional();
                    std::exit(0);
                }
                if (optional_str == "crash") {
                    opt_unwrap_mode = OptionalUnwrapMode::CRASH;
                } else if (optional_str == "unsafe") {
                    opt_unwrap_mode = OptionalUnwrapMode::UNSAFE;
                } else {
                    print_err("Unknown Mode: " + optional_str);
                    return 1;
                }
                i++;
            } else if (arg == "--variant") {
                if (!n_args_follow(i + 1, "<MODE>", arg)) {
                    return 1;
                }
                const std::string variant_str = args.at(i + 1);
                if (variant_str == "--help" || variant_str == "-h") {
                    print_help_variant();
                    std::exit(0);
                }
                if (variant_str == "crash") {
                    var_unwrap_mode = VariantUnwrapMode::CRASH;
                } else if (variant_str == "unsafe") {
                    var_unwrap_mode = VariantUnwrapMode::UNSAFE;
                } else {
                    print_err("Unknown Mode: " + variant_str);
                    return 1;
                }
                i++;
            } else if (arg.size() > 8 && arg.substr(0, 8) == "--flags=") {
                const std::string flags = arg.substr(8, arg.size() - 8);
                std::stringstream ss(flags);
                std::string line;
                while (std::getline(ss, line, ' ')) {
                    compile_flags.push_back(line);
                }
            } else if (arg == "--print-fip-version") {
                print_fip_version = true;
            } else if (arg == "--rebuild-core") {
                BUILTIN_LIBS_TO_PRINT = static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
            } else if (arg == "--print-libbuiltins-path") {
                std::cout << Generator::get_flintc_cache_path().string() << std::endl;
                return 1;
            } else if (arg == "--no-colors") {
                RED = "";
                GREEN = "";
                YELLOW = "";
                BLUE = "";
                CYAN = "";
                WHITE = "";
                DEFAULT = "";
                GREY = "";
                RED_UNDERLINE = "";
            } else if (arg == "--output-ll-file") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                ll_file_path = get_absolute(cwd_path, args.at(i + 1));
                build_exe = false;
                i++;
#ifdef DEBUG_BUILD
            } else if (arg == "--profile-cumulative") {
                PRINT_CUMULATIVE_PROFILE_RESULTS = true;
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
            } else if (arg == "--no-binary") {
                NO_BINARY = true;
            } else if (arg == "--print-file-ir") {
                PRINT_FILE_IR = true;
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
            } else if (arg == "--print-ir-math") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::MATH);
            } else if (arg == "--print-ir-parse") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::PARSE);
            } else if (arg == "--print-ir-time") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::TIME);
            } else if (arg == "--print-ir-dima") {
                BUILTIN_LIBS_TO_PRINT |= static_cast<unsigned int>(BuiltinLibrary::DIMA);
#endif
            } else {
                print_err("Unknown argument: " + arg);
                return 1;
            }
        }
        if (source_file_path.empty() && !print_version && !print_fip_version) {
            print_err("There is no file to compile!");
            return 1;
        }
        return 0;
    }

    std::filesystem::path source_file_path = "";
    std::filesystem::path out_file_path = "main";
    std::filesystem::path ll_file_path = "";
    std::vector<std::string> compile_flags;
    bool build_exe{true};
    bool run{false};
    bool test{false};
    bool parallel{false};
    bool is_static{false};
    bool print_version{false};
    bool print_fip_version{false};

  private:
    void print_help() override {
        std::cout << "Usage: flintc [OPTIONS]\n";
        std::cout << "\n";
        std::cout << "Available Options:\n";
        std::cout << "  -h, --help                      Show help\n";
        std::cout << "  -f, --file <file>               The file to compile\n";
        std::cout << "  -o, --out <file>                The name and path of the built output file\n";
        // If the --test flag is set, the compiler will output a test binary. The default name "main" is overwritten to
        // "test" in that case
        std::cout << "  -t, --test                      Output a test binary instead of the normal binary\n";
        // If the --run flag is set, the compiler will output the built binary into the .flintc directory.
        std::cout << "  -r, --run                       Run the built binary directly without outputting it\n";
        std::cout << "  -p, --parallel                  Compile in parallel (only recommended for bigger projects)\n";
        std::cout << "  -s, --static                    Build the executable as static\n";
        std::cout << "      --version                   Print the version of the compiler\n";
        std::cout << "      --target <TARGET>           Targets the given target platform (use --help for more information)\n";
        std::cout << "      --arithmetic <MODE>         Selecting the mode for arithmetic behaviour (use --help for more information)\n";
        std::cout << "      --array <MODE>              Selecting the mode for array behaviour (use --help for more information)\n";
        std::cout << "      --opaque <MODE>             Selecting the mode for opaque behaviour (use --help for more information)\n";
        std::cout << "      --optional <MODE>           Selecting the mode for optional behaviour (use --help for more information)\n";
        std::cout << "      --variant <MODE>            Selecting the mode for variant behaviour (use --help for more information)\n";
        std::cout << "      --flags=\"[FLAGS]*\"          The flags to pass to the linker (lld)\n";
        std::cout << "      --rebuild-core              Rebuild all the core modules\n";
        std::cout << "      --print-fip-version         Prints the version of the FIP this compiler uses\n";
        std::cout << "      --print-libbuiltins-path    Prints the path to the directory where the libbuiltins.a file is saved at\n";
        std::cout << "      --no-colors                 Disables colored console output\n";
        std::cout << "      --output-ll-file <file>     Whether to output the compiled IR code\n";
        std::cout << "                                  HINT: The compiler will not create an executable with this flag set\n";
#ifdef DEBUG_BUILD
        std::cout << YELLOW << "\nDebug Options" << DEFAULT << ":\n";
        std::cout
            << "      --profile-cumulative        Enables the cumulative profiling output, by default only the profile tree view is shown\n";
        std::cout << "      --hard-crash                Enables the option to hard crash the program in the case of a thrown error\n";
        std::cout << "      --no-token-stream           Disables the debug printing of the lexed Token stream\n";
        std::cout << "      --no-dep-tree               Disables the debug printing of the dependency tree\n";
        std::cout << "      --no-ast                    Disables the debug printing of the parsed AST tree\n";
        std::cout << "      --no-ir                     Disables the debug printing of the generated program IR code\n";
        std::cout << "      --no-profile                Disables the debug printing of the profiling results\n";
        std::cout << "      --no-generation             Disables code generation entirely, the program exits after the parsing phase\n";
        std::cout << "      --no-binary                 Disables compilation of the LLVM modules to a final binary, exiting after IR gen\n";
        std::cout << "                                  HINT: Doesnt produce an executable";
        std::cout << std::endl;
        std::cout << YELLOW << "\nIR printing Options" << DEFAULT << ":\n";
        std::cout << "      --print-file-ir             Enables printing of the IR code for every file which was parsed\n";
        std::cout << "      --print-ir-arithmetic       Enables printing of the IR code for the arithmetic.o library\n";
        std::cout << "                                  HINT: The arithmetic IR is not printed if '--arithmetic-unsafe' is used\n";
        std::cout << "      --print-ir-assert           Enables printing of the IR code for the assert.o library\n";
        std::cout << "      --print-ir-array            Enables printing of the IR code for the array.o library\n";
        std::cout << "      --print-ir-cast             Enables printing of the IR code for the cast.o library\n";
        std::cout << "      --print-ir-env              Enables printing of the IR code for the env.o library\n";
        std::cout << "      --print-ir-filesystem       Enables printing of the IR code for the fs.o library\n";
        std::cout << "      --print-ir-print            Enables printing of the IR code for the print.o library\n";
        std::cout << "      --print-ir-read             Enables printing of the IR code for the read.o library\n";
        std::cout << "      --print-ir-str              Enables printing of the IR code for the str.o library\n";
        std::cout << "      --print-ir-system           Enables printing of the IR code for the system.o library\n";
        std::cout << "      --print-ir-math             Enables printing of the IR code for the math.o library";
        std::cout << std::endl;
#endif
    }

    void print_help_targets() {
        std::cout << "Usage: flintc --target <TARGET>\n";
        std::cout << "\n";
        std::cout << "Available Targets:\n";
        std::cout << "  native      [Default] The native target triple of the platform the compiler is executed on\n";
        std::cout << "  linux       Targetting Linux (target triple 'x86_64-pc-linux-gnu')\n";
        std::cout << "  windows     Targetting Windows (target triple 'x86_64-pc-windows-gnu')\n";
        std::flush(std::cout);
    }

    void print_help_arithmetic() {
        std::cout << "Usage: flintc --arithmetic <MODE>\n";
        std::cout << "\n";
        std::cout << "  The arithmetic mode controls the behaviour of the compiled Flint program when an oveflow or\n";
        std::cout << "  underflow occurs. If an underflow or overflow happen respectively, the value is clamped to\n";
        std::cout << "  the minimum value (underflow) or the maximum value (overflow) respectively. This prevents\n";
        std::cout << "  wrap-arounds for various arithmetic operations in general\n";
        std::cout << "\n";
        std::cout << "  [NOTE]: The overflow / underflow checking is separate from the compiler optimization mode(s).\n";
        std::cout << "          It does not matter if you compile in release or debug mode, the behaviour of overflows\n";
        std::cout << "          and underflows is only controlled by this mode flag.\n";
        std::cout << "\n";
        std::cout << "  ┌─ Example Program ────────────────┐\n";
        std::cout << "  │ use Core.print                   │\n";
        std::cout << "  │                                  │\n";
        std::cout << "  │ def main():                      │\n";
        std::cout << "  │     u8 u = 255;                  │\n";
        std::cout << "  │     u += 1;                      │\n";
        std::cout << "  │     print($\"u = {u16(u)}\\n\");    │\n";
        std::cout << "  │                                  │\n";
        std::cout << "  │     u = 0;                       │\n";
        std::cout << "  │     u -= 1;                      │\n";
        std::cout << "  │     print($\"u = {u16(u)}\\n\");    │\n";
        std::cout << "  └──────────────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "Available Modes:\n";
        std::cout << "  print       [Default] Prints a small message to the console\n";
        std::cout << "                  The message will be printed to the console whenever an overflow or underflow\n";
        std::cout << "                  happens. The value will be clamped if an over/underflow happens.\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ u8 add overflow caught   │\n";
        std::cout << "              │ u = 255                  │\n";
        std::cout << "              │ u8 sub underflow caught  │\n";
        std::cout << "              │ u = 0                    │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "  silent      Disables the debug printing when an overflow or underflow happened\n";
        std::cout << "                  The value will still be clamped if an over/underflow happens but there will\n";
        std::cout << "                  be no message printed to the console if it happens, the clamping happens\n";
        std::cout << "                  silently\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ u = 255                  │\n";
        std::cout << "              │ u = 0                    │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "  crash       Hard crashes when an overflow / underflow occurred\n";
        std::cout << "                  There will be no clamping if an over/underflow happens, the runtime will\n";
        std::cout << "                  hard crash in this scenario. This option is most useful for testing, for\n";
        std::cout << "                  example if you are certain that no over/underflow would happen in you code\n";
        std::cout << "                  you can change the arithmetic mode to 'crash' to test that assumption.\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ u8 add overflow caught   │\n";
        std::cout << "              └─ Program Crashes ────────┘\n";
        std::cout << "\n";
        std::cout << "  unsafe      Disables all overflow and underflow checks to make arithmetic operations faster\n";
        std::cout << "                  This disables all over/underflow checks whatsoever. Arithmetic will be\n";
        std::cout << "                  emitted as unsafe IR code, and it's behaviour will be equivalent to the\n";
        std::cout << "                  wraparound behaviour of C, for example\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ u = 0                    │\n";
        std::cout << "              │ u = 255                  │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::flush(std::cout);
    }

    void print_help_array() {
        std::cout << "Usage: flintc --array <MODE>\n";
        std::cout << "\n";
        std::cout << "  The array mode controls what happens when an array is accessed out of it's bounds, e.g. when\n";
        std::cout << "  the index of the array access is greater or equal to the length of the array at that\n";
        std::cout << "  dimensionality. For all modes except for the 'unsafe' mode, accessing an array out of bounds\n";
        std::cout << "  results in the index being clamped to '0' at the lower end or the length of the dimensionality\n";
        std::cout << "  at the upper end respectively.\n";
        std::cout << "  This means that array accesses outside the bounds of the array access the first or last element\n";
        std::cout << "  of the array instead\n";
        std::cout << "\n";
        std::cout << "  [NOTE]: The out of bounds behaviour is separate from the compiler optimization mode(s). It does\n";
        std::cout << "          not matter if you compile in release or debug mode, the behaviour of out of bounds accesses\n";
        std::cout << "          is only controlled by this mode flag.\n";
        std::cout << "\n";
        std::cout << "  ┌─ Example Program ─────────────────────┐\n";
        std::cout << "  │ use Core.print                        │\n";
        std::cout << "  │                                       │\n";
        std::cout << "  │ def main():                           │\n";
        std::cout << "  │     i32[] arr = i32[5](0);            │\n";
        std::cout << "  │     for (i, elem) in arr:             │\n";
        std::cout << "  │         elem = i32(i);                │\n";
        std::cout << "  │                                       │\n";
        std::cout << "  │     print($\"arr[5] = {arr[5]}\\n\");    │\n";
        std::cout << "  │     print($\"arr[6] = {arr[6]}\\n\");    │\n";
        std::cout << "  └───────────────────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "Available Modes:\n";
        std::cout << "  print       [Default] Prints a small message to the console whenever accessing an array OOB\n";
        std::cout << "                  The index clamping behaviour is fully intact in this mode\n";
        std::cout << "              ┌─ Example Program Output ───────────────────────────┐\n";
        std::cout << "              │ Out Of Bounds access occured: Arr Len: 5, Index: 5 │\n";
        std::cout << "              │ arr[5] = 4                                         │\n";
        std::cout << "              │ Out Of Bounds access occured: Arr Len: 5, Index: 6 │\n";
        std::cout << "              │ arr[6] = 4                                         │\n";
        std::cout << "              └────────────────────────────────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "  silent      Disables the debug printing when OOB access happens\n";
        std::cout << "                  The index clamping behaviour is fully intact in this mode. The accessed element\n";
        std::cout << "                  still changes depending on the size of the array, but nothing is printed here.\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ arr[5] = 4               │\n";
        std::cout << "              │ arr[6] = 4               │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "  crash       Hard crashes when an OOB access happens\n";
        std::cout << "                  There will be no clamping behaviour of the accessed index of the array in this\n";
        std::cout << "                  mode. If an out of bounds access happens then the compiled program will hard\n";
        std::cout << "                  crash. This mode is best used to test whether any OOB access happens in the\n";
        std::cout << "                  program since it crashes instantly.\n";
        std::cout << "              ┌─ Example Program Output ───────────────────────────┐\n";
        std::cout << "              │ Out Of Bounds access occured: Arr Len: 5, Index: 5 │\n";
        std::cout << "              └─ Program Crashes ──────────────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "  unsafe      Disables all bounds checks for array accesses\n";
        std::cout << "                  There will be no safety checks around array bounds checking any more in this\n";
        std::cout << "                  mode. You can run into undefined behaviour, segmentation faults and all the\n";
        std::cout << "                  good stuff developers love about C. Since bounds checks are disabled the\n";
        std::cout << "                  program runs faster, but it is now considered unsafe.\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ arr[5] = 0               │\n";
        std::cout << "              │ arr[6] = 33              │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::flush(std::cout);
    }

    void print_help_opaque() {
        std::cout << "Usage: flintc --opaque <MODE>\n";
        std::cout << "  The opaque mode controls the behaviour when opaque memory is leaked, or at least considered to be\n";
        std::cout << "  leaked. Please look at " << BaseError::get_wiki_link() << "/beginners_guide/11_interop/7_opaque.html\n";
        std::cout << "  if you want to know more about the 'opaque' type itself.\n";
        std::cout << "\n";
        std::cout << "  [NOTE]: The opaque behaviour is separate from the compiler optimization mode(s). It does not\n";
        std::cout << "          matter if you compile in release or debug mode, the behaviour of opaque leak detection\n";
        std::cout << "          is only controlled by this mode flag.\n";
        std::cout << "\n";
        std::cout << "  ┌─ Example Program ───────────────────────┐\n";
        std::cout << "  │ use Core.print                          │\n";
        std::cout << "  │                                         │\n";
        std::cout << "  │ extern def malloc(mut u64 n) -> opaque; │\n";
        std::cout << "  │ extern def free(mut opaque ptr);        │\n";
        std::cout << "  │                                         │\n";
        std::cout << "  │ def main():                             │\n";
        std::cout << "  │     if true:                            │\n";
        std::cout << "  │         // Leaking                      │\n";
        std::cout << "  │         opaque memory = malloc(10);     │\n";
        std::cout << "  │         free(memory);                   │\n";
        std::cout << "  │     print(\"after leaking\\n\");           │\n";
        std::cout << "  │                                         │\n";
        std::cout << "  │     if true:                            │\n";
        std::cout << "  │         // Not Leaking                  │\n";
        std::cout << "  │         opaque memory = malloc(10);     │\n";
        std::cout << "  │         free(memory);                   │\n";
        std::cout << "  │         memory = null;                  │\n";
        std::cout << "  │     print(\"after not leaking\\n\");       │\n";
        std::cout << "  └─────────────────────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "Available Modes:\n";
        std::cout << "\n";
        std::cout << "  print       [Default] Prints a small message to the console whenever opaque memory is leaked\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ Error: Leaking memory!   │\n";
        std::cout << "              │ after leaking            │\n";
        std::cout << "              │ after not leaking        │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "  silent      Disables all leak checking for opaque types\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ after leaking            │\n";
        std::cout << "              │ after not leaking        │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "  crash       Hard crashes when opaque memory is leaked";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ Error: Leaking memory!   │\n";
        std::cout << "              └─ Program Crashes ────────┘\n";
        std::flush(std::cout);
    }

    void print_help_optional() {
        std::cout << "Usage: flintc --optional <MODE>\n";
        std::cout << "  The optional mode controls what happens when an optional force unwrap ( ! ) is executed on an\n";
        std::cout << "  optional typed value which does not contain any value, and holds the value of 'none' instead.\n";
        std::cout << "\n";
        std::cout << "  [NOTE]: The optional unwrap behaviour is separate from the compiler optimization mode(s). It\n";
        std::cout << "          does not matter if you compile in release or debug mode, the behaviour of unwrapping\n";
        std::cout << "          optional values is only controlled by this mode flag.\n";
        std::cout << "\n";
        std::cout << "  ┌─ Example Program ─────────┐\n";
        std::cout << "  │ use Core.print            │\n";
        std::cout << "  │                           │\n";
        std::cout << "  │ def main():               │\n";
        std::cout << "  │     i32? i = 10;          │\n";
        std::cout << "  │     print($\"i = {i!}\\n\"); │\n";
        std::cout << "  │     i = none;             │\n";
        std::cout << "  │     print($\"i = {i!}\\n\"); │\n";
        std::cout << "  └───────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "Available Modes:\n";
        std::cout << "  crash       [Default] Prints a small message and crashes whenever a bad optional unwrap happens\n";
        std::cout << "              ┌─ Example Program Output ─────┐\n";
        std::cout << "              │ i = 10                       │\n";
        std::cout << "              │ Bad optional access occurred │\n";
        std::cout << "              └─ Program Crashes ────────────┘\n";
        std::cout << "\n";
        std::cout << "  unsafe      Disables all \"has_value\"-checks for optionals when unwrapping\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ i = 10                   │\n";
        std::cout << "              │ i = 0                    │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::flush(std::cout);
    }

    void print_help_variant() {
        std::cout << "Usage: flintc --variant <MODE>\n";
        std::cout << "  The variant mode controls what happens when unwrapping a variant which holds a different typed\n";
        std::cout << "  value than the one which is force-unwrapped.\n";
        std::cout << "\n";
        std::cout << "  [NOTE]: The variant unwrap behaviour is separate from the compiler optimization mode(s). It\n";
        std::cout << "          does not matter if you compile in release or debug mode, the behaviour of unwrapping\n";
        std::cout << "          variant typed values is only controlled by this mode flag.\n";
        std::cout << "\n";
        std::cout << "  ┌─ Example Program ──────────────┐\n";
        std::cout << "  │ use Core.print                 │\n";
        std::cout << "  │                                │\n";
        std::cout << "  │ variant MyVariant:             │\n";
        std::cout << "  │     i32, f32;                  │\n";
        std::cout << "  │                                │\n";
        std::cout << "  │ def main():                    │\n";
        std::cout << "  │     MyVariant v = i32(10);     │\n";
        std::cout << "  │     print($\"v = {v!(i32)}\\n\"); │\n";
        std::cout << "  │     print($\"v = {v!(f32)}\\n\"); │\n";
        std::cout << "  └────────────────────────────────┘\n";
        std::cout << "\n";
        std::cout << "Available Modes:\n";
        std::cout << "  crash       [Default] Prints a small message and crashes whenever a bad variant unwrap happens\n";
        std::cout << "              ┌─ Example Program Output ────┐\n";
        std::cout << "              │ v = 10                      │\n";
        std::cout << "              │ Bad variant unwrap occurred │\n";
        std::cout << "              └─ Program Crashes ───────────┘\n";
        std::cout << "\n";
        std::cout << "  unsafe      Disables all \"is_type\"-checks for variants when unwrapping\n";
        std::cout << "                  When unwrapping variants in the 'unsafe' mode the compiler will not emit any checks\n";
        std::cout << "                  to look whether the variant actually holds the type which is unwrapped. In the case\n";
        std::cout << "                  of the example the bits of the 'i32' value are just reinterpreted as a 'f32' value.\n";
        std::cout << "              ┌─ Example Program Output ─┐\n";
        std::cout << "              │ v = 10                   │\n";
        std::cout << "              │ v = 1.401298e-44         │\n";
        std::cout << "              └──────────────────────────┘\n";
        std::flush(std::cout);
    }
};
