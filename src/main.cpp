#include "cli_parser_main.hpp"
#include "debug.hpp"
#include "generator/generator.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

/// build_executable
///     Builds the executable file from the "output.ll" file
void build_executable(const std::filesystem::path &ll_file_path, const std::filesystem::path &binary_path, const std::string &flags) {
    if (std::filesystem::exists(ll_file_path)) {
        std::cout << "Using clang to build the executable '" << binary_path.filename().string() << "' ..." << std::endl;
        system(std::string("clang " + flags + " " + ll_file_path.string() + " -o " + binary_path.string()).c_str());
        std::filesystem::remove(ll_file_path);
    }
}

/// generate_ll_file
///     Generates the "output.ll" file from a given source file
void generate_ll_file(const std::filesystem::path &source_file_path, const std::filesystem::path &out_file_path,
    const std::filesystem::path &ll_file_path) {
    // Parse the .ft file and resolve all inclusions
    FileNode file = Parser::parse_file(source_file_path);
    Debug::AST::print_file(file);
    std::shared_ptr<DepNode> dep_graph = Resolver::create_dependency_graph(file, source_file_path.parent_path());
    Parser::resolve_call_types();

    // Generate the whole program
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> program = Generator::generate_program_ir("main", context, dep_graph);

    // Write the code to the normal output.bc file
    std::error_code EC;
    llvm::raw_fd_ostream bc_file(out_file_path.string(), EC);
    if (!EC) {
        llvm::WriteBitcodeToFile(*program, bc_file);
        bc_file.close();
    }

    // Write the code to the declared .ll file, if it is declared
    if (!ll_file_path.empty()) {
        llvm::raw_fd_ostream ll_file(ll_file_path.string(), EC);
        if (!EC) {
            ll_file << Generator::resolve_ir_comments(Generator::get_module_ir_string(program.get()));
            ll_file.close();
        }
    }

    // Clean up the module
    Resolver::clear();
    program->dropAllReferences();
    program.reset();
}

int main(int argc, char *argv[]) {
    CLIParserMain clp(argc, argv);
    int result = clp.parse();
    if (result != 0) {
        return result;
    }
    generate_ll_file(clp.source_file_path, "output.ll", clp.ll_file_path);
    if (clp.build_exe) {
        build_executable("output.ll", clp.out_file_path, clp.compile_flags);
    }
    return 0;
}
