#include "cli_parser_main.hpp"
#include "debug.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#include "linker_interface.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/parser.hpp"
#include "profiler.hpp"
#include "resolver/resolver.hpp"

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

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

/// @function `generate_module`
/// @brief Generates the whole module from a given source file
///
/// @param `source_file_path` The source file to generate the module from
/// @param `is_test` Whether the program has to be generated in test-mode
/// @param `parse_parallel` Whether to parse the program in parallel
/// @param `context` The LLVMContext in which to generate the module in
/// @return `std::optional<std::unique_ptr<llvm::Module>>` The generated module, nullopt if generation failed
std::optional<std::unique_ptr<llvm::Module>> generate_module( //
    const std::filesystem::path &source_file_path,            //
    const bool is_test,                                       //
    const bool parse_parallel,                                //
    llvm::LLVMContext &context                                //
) {
    PROFILE_SCOPE("Generate module");

    // Parse the .ft file and resolve all inclusions
    std::optional<FileNode> file = Parser::create(source_file_path)->parse();
    if (!file.has_value()) {
        std::cerr << "Error: Failed to parse file '" << source_file_path.filename() << "'" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    auto dep_graph = Resolver::create_dependency_graph(file.value(), source_file_path.parent_path(), parse_parallel);
    if (!dep_graph.has_value()) {
        std::cerr << "Error: Failed to create dependency graph" << std::endl;
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
    return Generator::generate_program_ir(is_test ? "test" : "main", context, dep_graph.value(), is_test);
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

/// @function `compile_module`
/// @brief Compiles the given module down to a binary
///
/// @param `binary_file` The path where the built binary should be placed at
/// @param `module` The program to compile
void compile_module(const std::filesystem::path &binary_file, llvm::Module *module, const bool is_static) {
    PROFILE_SCOPE("Compile module " + module->getName().str());
    // Initialize LLVM targets (call this once in your compiler)
    static bool initialized = false;
    if (!initialized) {
        PROFILE_SCOPE("Initialize LLVM");
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86Target();
        LLVMInitializeX86TargetMC();
        LLVMInitializeX86AsmParser();
        LLVMInitializeX86AsmPrinter();
        initialized = true;
    }

    // Get the target triple (architecture, OS, etc.)
#if defined(__WIN32__)
    std::string target_triple = "x86_64-pc-windows-msvc";
#else
    std::string target_triple = llvm::sys::getDefaultTargetTriple();
#endif

    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Target triple information" << DEFAULT << "\n" << target_triple << std::endl;
    }
    module->setTargetTriple(target_triple);

    std::string error;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        llvm::errs() << "Error: " << error << "\n";
        return;
    }

    // Create the target machine
    llvm::TargetOptions opt;
    auto target_machine = target->createTargetMachine(target_triple, llvm::sys::getHostCPUName(), "", opt, llvm::Reloc::PIC_);
    module->setDataLayout(target_machine->createDataLayout());

    // Create an output file
    std::error_code EC;
#if defined(__WIN32__)
    const std::string obj_file = binary_file.string() + ".obj";
#else
    const std::string obj_file = binary_file.string() + ".o";
#endif
    llvm::raw_fd_ostream dest(obj_file, EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message() << "\n";
        return;
    }

    // Set up the pass manager and code generation
    llvm::legacy::PassManager pass;
    llvm::CodeGenFileType fileType = llvm::CodeGenFileType::ObjectFile;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
        llvm::errs() << "TargetMachine can't emit a file of this type!\n";
        return;
    }

    // Run the passes to generate machine code
    Profiler::start_task("Generate machine code");
    pass.run(*module);
    dest.flush(); // Ensure the file is written
    Profiler::end_task("Generate machine code");

    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Code generation status" << DEFAULT << std::endl;
        llvm::outs() << "-- Machine code generated: " << obj_file << "\n\n";
    }

    // Direct linking with LDD
    PROFILE_SCOPE("Creating the binary '" + binary_file.filename().string() + "'");

    bool link_success = LinkerInterface::link(obj_file, // input object file
        binary_file,                                    // output executable
        is_static                                       // debug flag
    );

    if (!link_success) {
        llvm::errs() << "Linking failed with LLD\n";
        return;
    }

    // Clean up object file
    if (!DEBUG_MODE) {
        std::filesystem::remove(std::filesystem::path(obj_file));
    }

    // OLD LINKING STUFF
    // Link the object file to create an executable using gcc to use the provided c runtime
    // #if defined(__WIN32__)
    //     std::string link_command = "link " + binary_file.string() + ".obj /OUT:" + binary_file.string() + ".exe
    //     /DEFAULTLIB:libcmt.lib";
    // #else
    //     std::string link_command = "gcc " + binary_file.string() + ".o -o " + binary_file.string() + " -lc";
    // #endif
    //     PROFILE_SCOPE("Creating the binary '" + binary_file.filename().string() + "'");

    //     if (DEBUG_MODE) {
    //         std::cout << YELLOW << "[Debug Info] Link command: " << DEFAULT << "\n" << link_command << "\n" << std::endl;
    //     }
    //     int link_result = std::system(link_command.c_str());
    //     if (link_result != 0) {
    //         llvm::errs() << "Linking failed with command: " << link_command << "\n";
    //         return;
    //     }
}

int main(int argc, char *argv[]) {
    Profiler::start_task("ALL");
    CLIParserMain clp(argc, argv);
    int result = clp.parse();
    if (result != 0) {
        return result;
    }
    llvm::LLVMContext context;
    auto program = generate_module(clp.source_file_path, clp.test, clp.parallel, context);
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
        // Compile the module and output the binary
        compile_module(clp.out_file_path, program.value().get(), clp.is_static);
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
