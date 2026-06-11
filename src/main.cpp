#include "cli_parser_main.hpp"
#include "fip.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#include "io.hpp"
#include "linker/linker.hpp"
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

std::filesystem::path main_file_path;

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
/// @param `flags` The flags used during compilation and linking
/// @param `is_static` Whether the program
/// @return `bool` Whether compilation of the program was successful
bool compile_program(                         //
    const std::filesystem::path &binary_file, //
    llvm::Module *module,                     //
    const std::vector<std::string> &flags,    //
    const bool is_static                      //
) {
    PROFILE_SCOPE("Compile program " + module->getName().str());

    // Direct linking with LDD
    if (!Generator::compile_module(module, binary_file)) {
        llvm::errs() << "Compilation of program '" << binary_file.string() << "' failed\n";
        return false;
    }

    std::string obj_file = binary_file.string();
    std::string file_ending = "";
    switch (COMPILATION_TARGET) {
        case Target::NATIVE:
#ifdef __WIN32__
            file_ending = ".obj";
#else
            file_ending = ".o";
#endif
            break;
        case Target::LINUX:
            file_ending = ".o";
            break;
        case Target::WINDOWS:
            file_ending = ".obj";
            break;
    }
    obj_file += file_ending;

    Profiler::start_task("Linking " + obj_file + " to a binary");
    std::vector<std::filesystem::path> obj_files{obj_file};
    std::optional<std::vector<std::array<char, 9>>> fip_objects = FIP::gather_objects();
    if (!fip_objects.has_value()) {
        Profiler::end_task("Linking " + obj_file + " to a binary");
        remove_with_retry(std::filesystem::path(obj_file));
        return false;
    }
    for (const auto &fip_obj : fip_objects.value()) {
        std::string fip_obj_path = ".fip/cache/" + std::string(fip_obj.data()) + file_ending;
        obj_files.emplace_back(fip_obj_path);
    }
    bool link_success = Linker::link( //
        obj_files,                    // input object files
        binary_file,                  // output executable
        flags,                        // linking flags
        is_static                     // debug flag
    );
    Profiler::end_task("Linking " + obj_file + " to a binary");

    if (!link_success) {
        llvm::errs() << "Linking failed with LLD\n";
        return false;
    }

    // Clean up object file
    if (!DEBUG_MODE) {
        remove_with_retry(std::filesystem::path(obj_file));
    }
    return true;
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
        std::cout << "flintc " << VERSION << " (" << COMMIT_HASH << ", " << BUILD_DATE << ")";
        if (DEBUG_MODE) {
            std::cout << " [debug]";
        }
        std::cout << std::endl;
        std::cout << " └─ Flint Interop Protocol v" << FIP_MAJOR << "." << FIP_MINOR << "." << FIP_PATCH << std::endl;
        return 0;
    }

    main_file_path = clp.source_file_path;

    Profiler::start_task("ALL");
    const auto &dep_graph = Parser::parse_program(clp.source_file_path, clp.test, clp.parallel);
    if (!dep_graph.has_value()) {
        Profiler::end_task("ALL");
        FIP::shutdown();
        return 1;
    }

    // Skip generation if requested
    if (DEBUG_MODE && NO_GENERATION) {
        FIP::shutdown();
        Profiler::end_task("ALL");
        if (PRINT_PROFILE_RESULTS) {
            Profiler::print_results(Profiler::TimeUnit::MICS);
        }
        if (PRINT_CUMULATIVE_PROFILE_RESULTS) {
            Profiler::print_cumulative_stats("total");
        }
        return 0;
    }

    // Send the compile request to all interop modules so they all compile their sources
    FIP::send_compile_request();

    // Generate the whole program
    auto program = Generator::generate_program_ir(clp.test ? "test" : "main", dep_graph.value(), clp.test);
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
        if (!NO_BINARY && !compile_program(clp.out_file_path, program.value().get(), clp.compile_flags, clp.is_static)) {
            Resolver::clear();
            FIP::shutdown();
            Profiler::end_task("ALL");
            program.value()->dropAllReferences();
            program.value().reset();
            return 1;
        }
    }

    Resolver::clear();
    FIP::shutdown();
    Profiler::end_task("ALL");
    if (PRINT_PROFILE_RESULTS) {
        Profiler::print_results(Profiler::TimeUnit::MICS);
    }
    if (PRINT_CUMULATIVE_PROFILE_RESULTS) {
        Profiler::print_cumulative_stats("total");
    }
    program.value()->dropAllReferences();
    program.value().reset();

    if (clp.run) {
        if (DEBUG_MODE) {
            std::cout << "\n"
                      << YELLOW << "[Debug Info] Running the executable '" << clp.out_file_path.string() << "'" << DEFAULT << std::endl;
        }
#ifdef __WIN32__
        const std::string system_command(std::string(".\\" + clp.out_file_path.string() + ".exe"));
#else
        const std::string system_command(std::string("./" + clp.out_file_path.string()));
#endif
        return system(system_command.c_str());
    }
    return 0;
}
