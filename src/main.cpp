#include "cli_parser_main.hpp"
#include "fip.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
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

    if (clp.output_ll_file) {
        PROFILE_SCOPE("Write the ll file");
        std::error_code EC;
        llvm::raw_fd_ostream ll_file(clp.ll_file_path.string(), EC);
        if (!EC) {
            ll_file << Generator::resolve_ir_comments(Generator::get_module_ir_string(program.value().get()));
            ll_file.close();
        }
    } else {
        // Compile the program and output the binary
        if (!NO_BINARY && !Generator::compile_program(clp.out_file_path, program.value().get(), clp.compile_flags, clp.is_static)) {
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
