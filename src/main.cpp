#include "cli_parser_main.hpp"
#include "debug.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#include "linker/linker.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/parser.hpp"
#include "profiler.hpp"
#include "resolver/resolver.hpp"

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

/// @function `compile_extern`
/// @brief Compiles the program using an external tool like clang or zig cc
///
/// @param `binary_path` The path to the binary output of the compilation
/// @param `compile_command` The compile command with which to compile the program
/// @param `flags` The flags added to the compile command
/// @param `module` The llvm module to compile
void compile_extern(                          //
    const std::filesystem::path &binary_path, //
    const std::string &compile_command,       //
    const std::string &flags,                 //
    llvm::Module *module                      //
) {
    PROFILE_SCOPE("Compile extern with '" + compile_command + "'");
    // First, write the module to the .bc file
    std::error_code EC;
    const std::filesystem::path bc_path = binary_path.root_path() / "program.bc";
    llvm::raw_fd_ostream bc_file(bc_path.string(), EC);
    if (!EC) {
        llvm::WriteBitcodeToFile(*module, bc_file);
        bc_file.close();
    }

    std::cout << "Using '" + compile_command + "' to build the executable '" << binary_path.filename().string() << "' ..." << std::endl;
    std::string compile_command_str = compile_command + " " + flags + " " + bc_path.string() + " -o " + binary_path.string();
    const auto [res, output] = CLIParserBase::get_command_output(compile_command_str.c_str());
    if (res != 0) {
        // Throw an error stating that compilation failed
        std::cerr << "The compilation using the command '" << compile_command_str << "' failed.\n"
                  << "\nThe output of the compilation:\n"
                  << output << std::endl;
        return;
    } else {
        // Only remove the ll file if it has compiled successfully. If it did fail compilation, keep the file for further investigations
        std::filesystem::remove(bc_path);
        // Print the compile output if in debug mode
        if (DEBUG_MODE) {
            std::cout << YELLOW << "[Debug Info] Compiler output: \n" << DEFAULT << output << std::endl;
        }
    }
    std::cout << "Build successful" << std::endl;
}

/// @function `generate_program`
/// @brief Generates the whole program from a given source file
///
/// @param `source_file_path` The source file to generate the module from
/// @param `is_test` Whether the program has to be generated in test-mode
/// @param `parse_parallel` Whether to parse the program in parallel
/// @param `context` The LLVMContext in which to generate the module in
/// @return `std::optional<std::unique_ptr<llvm::Module>>` The generated module, nullopt if generation failed
std::optional<std::unique_ptr<llvm::Module>> generate_program( //
    const std::filesystem::path &source_file_path,             //
    const bool is_test,                                        //
    const bool parse_parallel                                  //
) {
    PROFILE_SCOPE("Generate module");

    // Parse the .ft file and resolve all inclusions
    Type::init_types();
    Resolver::add_path(source_file_path.filename().string(), source_file_path.parent_path());
    std::optional<FileNode> file = Parser::create(source_file_path)->parse();
    if (!file.has_value()) {
        std::cerr << RED << "Error" << DEFAULT << ": Failed to parse file " << YELLOW << source_file_path.filename() << DEFAULT
                  << std::endl;
        std::exit(EXIT_FAILURE);
    }
    auto dep_graph = Resolver::create_dependency_graph(file.value(), source_file_path.parent_path(), parse_parallel);
    if (!dep_graph.has_value()) {
        std::cerr << RED << "Error" << DEFAULT << ": Failed to create dependency graph" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    if (PRINT_DEP_TREE) {
        Debug::Dep::print_dep_tree(0, dep_graph.value());
    }
    bool parsed_successful = Parser::parse_all_open_functions(parse_parallel);
    if (!parsed_successful) {
        return std::nullopt;
    }
    if (is_test) {
        bool parsed_tests_successful = Parser::parse_all_open_tests(parse_parallel);
        if (!parsed_tests_successful) {
            return std::nullopt;
        }
    }
    Parser::clear_instances();
    if (PRINT_AST) {
        Debug::AST::print_all_files();
    }

    // Generate the whole program
    return Generator::generate_program_ir(is_test ? "test" : "main", dep_graph.value(), is_test);
}

/// @function `write_ll_file`
/// @brief Simply writes the given module to the given file, in IR source format
///
/// @param `ll_path` The path where to write the IR code to
/// @param `module` The module containing the program to write to the given file
void write_ll_file(const std::filesystem::path &ll_path, const llvm::Module *module) {
    PROFILE_SCOPE("Write the ll file");
    std::error_code EC;
    llvm::raw_fd_ostream ll_file(ll_path.string(), EC);
    if (!EC) {
        ll_file << Generator::resolve_ir_comments(Generator::get_module_ir_string(module));
        ll_file.close();
    }
}

/// @function `compile_program`
/// @brief Compiles the given program module down to a binary
///
/// @param `binary_file` The path where the built binary should be placed at
/// @param `module` The program to compile
void compile_program(const std::filesystem::path &binary_file, llvm::Module *module, const bool is_static) {
    PROFILE_SCOPE("Compile program " + module->getName().str());

    // Direct linking with LDD
    PROFILE_SCOPE("Creating the binary '" + binary_file.filename().string() + "'");
    if (!Generator::compile_module(module, binary_file)) {
        llvm::errs() << "Compilation of program '" << binary_file.string() << "' failed\n";
        return;
    }

#ifdef __WIN32__
    const std::string obj_file = binary_file.string() + ".obj";
#else
    const std::string obj_file = binary_file.string() + ".o";
#endif

    bool link_success = Linker::link(obj_file, // input object file
        binary_file,                           // output executable
        is_static                              // debug flag
    );

    if (!link_success) {
        llvm::errs() << "Linking failed with LLD\n";
        return;
    }

    // Clean up object file
    if (!DEBUG_MODE) {
        std::filesystem::remove(std::filesystem::path(obj_file));
    }
}

int main(int argc, char *argv[]) {
    Profiler::start_task("ALL");
    CLIParserMain clp(argc, argv);
    int result = clp.parse();
    if (result != 0) {
        return result;
    }
#ifdef __WIN32__
    // Setting the console output to UTF-8 that the tree characters render correctly
    SetConsoleOutputCP(CP_UTF8);
#endif

    auto program = generate_program(clp.source_file_path, clp.test, clp.parallel);
    if (!program.has_value()) {
        return 1;
    }

    if (!clp.build_exe) {
        // Output the built module and write it to the given file
        write_ll_file(clp.ll_file_path, program.value().get());
        // } else if (clp.run) {
        // Run the IR code idrectly through JIT compilation
        // TODO
    } else if (!clp.compile_command.empty()) {
        // Compile the module with the correct compiler
        compile_extern(clp.out_file_path, clp.compile_command, clp.compile_flags, program.value().get());
    } else {
        // Compile the program and output the binary
        compile_program(clp.out_file_path, program.value().get(), clp.is_static);
    }

    Resolver::clear();
    Profiler::end_task("ALL");
    if (PRINT_PROFILE_RESULTS) {
        Profiler::print_results(Profiler::TimeUnit::MICS);
    }
    program.value()->dropAllReferences();
    program.value().reset();

    if (clp.run) {
        std::cout << "\n--- Running the executable '" << clp.out_file_path.string() << "' ---" << std::endl;
        return system(std::string("./" + clp.out_file_path.string()).c_str());
    }
    return 0;
}
