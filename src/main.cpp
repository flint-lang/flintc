#include "parser/ast/program_node.hpp"
#include "parser/parser.hpp"
#include "tests.hpp"

#include <string>
#include <filesystem> // to get cwd
#include <iostream>

class CommandLineParser {
    public:
        CommandLineParser(int argc, char* argv[]) {
            // Convert the char* argv[] to a vector of strings
            args.reserve(argc - 1);
            for(size_t i = 1; i < argc; ++i) {
                args.push_back(std::string(argv[i]));
            }
        }

        int parse() {
            // Iterate through all command-line arguments
            for(size_t i = 0; i < args.size(); ++i) {
                std::string arg = args[i];

                if(arg == "--help" || arg == "-h") {
                    print_help();
                } else if(arg == "--test" || arg == "-t") {
                    int fails = test_all();
                } else if(arg == "--file" || arg == "-f") {
                    if(!n_args_follow(i + 1, "<file>", arg)) {
                        return 1;
                    }
                    std::filesystem::path cwd_path = std::filesystem::current_path();
                    std::string cwd = cwd_path.string() + "/";
                    std::string file_path = args.at(i + 1);

                    ProgramNode program;
                    Parser::parse_file(program, file_path);
                    ++i;
                } else if(arg == "-o") {
                    if(!n_args_follow(i + 1, "<file>", arg)) {
                        return 1;
                    }
                } else {
                    print_err("Unknown argument: " + arg);
                    return 1;
                }
            }
            return 0;
        }

    private:
        std::vector<std::string> args;
        std::string argument_value;

        static void print_help() {
            std::cout << "Usage: [OPTIONS]\n";
            std::cout << "--help, -h            Show help\n";
            std::cout << "--test, -t            Run all builtin unit tests\n";
            std::cout << "--file, -f <file>     The file to compile\n";
            std::cout << "-o <file>             The name and path of the built output file\n";
        }

        static void print_err(const std::string &err) {
            std::cerr << err << "\n";
            print_help();
        }

        bool n_args_follow(uint count, const std::string &arg, const std::string &option) {
            if(args.size() <= count) {
                std::cerr << "Expected " << arg << " after '" << option << "' option!\n";
                print_help();
                return false;
            }
            return true;
        }
};

int main(int argc, char* argv[]) {
    CommandLineParser command_line_parser(argc, argv);
    return command_line_parser.parse();
}
