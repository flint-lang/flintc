#include "generator/generator.hpp"
#include "linker/linker.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <filesystem> // to get cwd
#include <iostream>
#include <memory>
#include <string>
#include <sys/types.h>
#include <system_error>

void build_executable(const std::string &file_name) {
    std::filesystem::path ll_file = std::filesystem::current_path() / "output.ll";
    if (std::filesystem::exists(ll_file)) {
        std::cout << "Using clang to build the executable '" << file_name << "' ..." << std::endl;
        system(std::string("clang output.ll -o " + file_name).c_str());
        // std::filesystem::remove(ll_file);
    }
}

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
        for (size_t i = 0; i < args.size(); ++i) {
            std::string arg = args[i];

            if (arg == "--help" || arg == "-h") {
                print_help();
            } else if (arg == "--file" || arg == "-f") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                std::filesystem::path cwd_path = std::filesystem::current_path();
                std::filesystem::path file_path = cwd_path / args.at(i + 1);
                std::string test_path = file_path.parent_path();

                FileNode file = Parser::parse_file(file_path);
                Linker::resolve_links(file, file_path.parent_path());

                llvm::LLVMContext context;
                std::unique_ptr<llvm::Module> program = Generator::generate_program_ir("main", &context);
                // std::cout << "IR_CODE:\n" << Generator::get_module_ir_string(program.get()) << std::endl;

                // Write the code to a .ll file
                std::error_code EC;
                llvm::raw_fd_ostream ll_file("output.ll", EC);
                if (!EC) {
                    program->print(ll_file, nullptr);
                    ll_file.close();
                }

                // Clean up the module
                Resolver::clear();
                program->dropAllReferences();
                program.reset();
                ++i;
            } else if (arg == "-o") {
                if (!n_args_follow(i + 1, "<file>", arg)) {
                    return 1;
                }
                build_executable(args[i + 1]);
                ++i;
            } else {
                print_err("Unknown argument: " + arg);
                return 1;
            }
        }
        build_executable("main");
        return 0;
    }

  private:
    std::vector<std::string> args;
    std::string argument_value;

    static void print_help() {
        std::cout << "Usage: [OPTIONS]\n";
        std::cout << "--help, -h            Show help\n";
        std::cout << "--file, -f <file>     The file to compile\n";
        std::cout << "-o <file>             The name and path of the built output file\n";
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

int main(int argc, char *argv[]) {
    CommandLineParser command_line_parser(argc, argv);
    return command_line_parser.parse();
}
