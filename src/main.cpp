#include "cli_parser_main.hpp"
#include "debug.hpp"
#include "generator/generator.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/parser.hpp"
#include "profiler.hpp"
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
void build_executable(                         //
    const std::filesystem::path &ll_file_path, //
    const std::filesystem::path &binary_path,  //
    const std::string &compile_command,        //
    const std::string &flags                   //
) {
    PROFILE_SCOPE("Compiling with '" + compile_command + "'");
    if (std::filesystem::exists(ll_file_path)) {
        std::cout << "Using '" + compile_command + "' to build the executable '" << binary_path.filename().string() << "' ..." << std::endl;
        std::string compile_command_str = compile_command + " " + flags + " " + ll_file_path.string() + " -o " + binary_path.string();
        const auto [res, output] = CLIParserBase::get_command_output(compile_command_str.c_str());
        if (res != 0) {
            // Throw an error stating that compilation failed
            std::cerr << "The compilation failed using the command '" << compile_command_str << "'.\nLook at the '" << ll_file_path.string()
                      << "' file and try to compile it using a different method!\n\nThe output of the compilation was the following:\n"
                      << output << std::endl;
            return;
        } else {
            // Only remove the ll file if it has compiled successfully. If it did fail compilation, keep the file for further investigations
            std::filesystem::remove(ll_file_path);
        }
        std::cout << "Build successful" << std::endl;
    }
}

/// generate_ll_file
///     Generates the "output.ll" file from a given source file
void generate_ll_file(const std::filesystem::path &source_file_path, const std::filesystem::path &out_file_path,
    const std::filesystem::path &ll_file_path) {
    PROFILE_SCOPE("'generate_ll_file'");

    // Parse the .ft file and resolve all inclusions
    std::optional<FileNode> file = Parser(source_file_path).parse();
    if (!file.has_value()) {
        std::cerr << "Error: Failed to parse file '" << source_file_path.filename() << "'" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    Debug::AST::print_file(file.value());
    auto dep_graph = Resolver::create_dependency_graph(file.value(), source_file_path.parent_path());
    if (!dep_graph.has_value()) {
        std::cerr << "Error: Failed to create dependency graph" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    Debug::Dep::print_dep_tree(0, dep_graph.value());
    Parser::resolve_call_types();

    // Generate the whole program
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> program;
    program = Generator::generate_program_ir("main", context, dep_graph.value());

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
    generate_ll_file(clp.source_file_path, "output.bc", clp.ll_file_path);
    if (clp.build_exe) {
        build_executable("output.bc", clp.out_file_path, clp.compile_command, clp.compile_flags);
    }
    Profiler::print_results(Profiler::TimeUnit::MICS);
    return 0;
}
