#include "cli_parser_main.hpp"
#include "debug.hpp"
#include "fip.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#include "lexer/lexer.hpp"
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
    ScopeProfiler sp("Generate module");

    // Parse the .ft file and resolve all inclusions
    Profiler::start_task("Parsing the program", true);
    Type::init_types();
    Resolver::add_path(source_file_path.filename().string(), source_file_path.parent_path());
    std::optional<FileNode *> file = Parser::create(source_file_path)->parse();
    if (!file.has_value()) {
        std::cerr << RED << "Error" << DEFAULT << ": Failed to parse file " << YELLOW << source_file_path.filename() << DEFAULT
                  << std::endl;
        return std::nullopt;
    }
    auto dep_graph = Resolver::create_dependency_graph(file.value(), source_file_path.parent_path(), parse_parallel);
    if (!dep_graph.has_value()) {
        std::cerr << RED << "Error" << DEFAULT << ": Failed to create dependency graph" << std::endl;
        return std::nullopt;
    }
    if (!Parser::main_function_parsed && !is_test) {
        // No main function found
        THROW_ERR(ErrDefNoMainFunction, ERR_PARSING, source_file_path.filename().string());
        return std::nullopt;
    }
    Parser::resolve_all_unknown_types();
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
    Profiler::end_task("Parsing the program");
    if (PRINT_AST) {
        Debug::AST::print_all_files();
    }
    if (DEBUG_MODE) {
        const unsigned int token_count = Lexer::total_token_count;
        const ProfileNode *const parse_node = Profiler::profiling_durations.at("Parsing the program");
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(parse_node->end - parse_node->start);
        std::cout << YELLOW << "[Debug Info] Token parsing performance\n"
                  << DEFAULT << "-- Total token count: " << token_count << "\n"
                  << "-- Total parsing time: " << std::to_string(duration.count()) << " µs\n"
                  << "-- Tokens per second parsing speed: " << ((token_count * 1000000) / duration.count()) << " Tok/s\n"
                  << std::endl;
    }
    if (DEBUG_MODE && NO_GENERATION) {
        sp.~ScopeProfiler();
        Profiler::end_task("ALL");
        if (PRINT_PROFILE_RESULTS) {
            Profiler::print_results(Profiler::TimeUnit::MICS);
        }
        exit(0);
    }

    // Now we can send the compile request to all interop modules
    FIP::send_compile_request();

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
    if (!Generator::compile_module(module, binary_file)) {
        llvm::errs() << "Compilation of program '" << binary_file.string() << "' failed\n";
        return;
    }

    std::string obj_file = binary_file.string();
    switch (COMPILATION_TARGET) {
        case Target::NATIVE:
#ifdef __WIN32__
            obj_file += ".obj";
#else
            obj_file += ".o";
#endif
            break;
        case Target::LINUX:
            obj_file += ".o";
            break;
        case Target::WINDOWS:
            obj_file += ".obj";
            break;
    }

    Profiler::start_task("Linking " + obj_file + " to a binary");
    bool link_success = Linker::link(obj_file, // input object file
        binary_file,                           // output executable
        is_static                              // debug flag
    );
    Profiler::end_task("Linking " + obj_file + " to a binary");

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
    // Parse all the cli arguments
    CLIParserMain clp(argc, argv);
    int result = clp.parse();
    if (result != 0) {
        return result;
    }
#ifdef __WIN32__
    // Setting the console output to UTF-8 that the tree characters render correctly
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Print the version if requested
    if (clp.print_version) {
        std::cout << "flintc " << MAJOR << "." << MINOR << "." << PATCH << "-" << VERSION << " (" << COMMIT_HASH_VALUE << ", " << BUILD_DATE
                  << ")";
        if (DEBUG_MODE) {
            std::cout << " [debug]";
        }
        std::cout << std::endl;
        return 0;
    }

    Profiler::start_task("ALL");
    if (!FIP::init()) {
        Profiler::end_task("ALL");
        return 1;
    }
    auto program = generate_program(clp.source_file_path, clp.test, clp.parallel);
    if (!program.has_value()) {
        Profiler::end_task("ALL");
        FIP::shutdown();
        return 1;
    }
    Parser::clear_instances();

    if (!clp.build_exe) {
        // Output the built module and write it to the given file
        write_ll_file(clp.ll_file_path, program.value().get());
        // } else if (clp.run) {
        // Run the IR code idrectly through JIT compilation
        // TODO
    } else {
        // Compile the program and output the binary
        compile_program(clp.out_file_path, program.value().get(), clp.is_static);
    }

    Resolver::clear();
    FIP::shutdown();
    Profiler::end_task("ALL");
    if (PRINT_PROFILE_RESULTS) {
        Profiler::print_results(Profiler::TimeUnit::MICS);
    }
    program.value()->dropAllReferences();
    program.value().reset();

    if (clp.run) {
        std::cout << "\n--- Running the executable '" << clp.out_file_path.string() << "' ---" << std::endl;
#ifdef __WIN32__
        const std::string system_command(std::string(".\\" + clp.out_file_path.string() + ".exe"));
#else
        const std::string system_command(std::string("./" + clp.out_file_path.string()));
#endif
        return system(system_command.c_str());
    }
    return 0;
}
