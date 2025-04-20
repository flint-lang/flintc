#include "linker/linker.hpp"
#include "colors.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"

#include <lld/Common/Driver.h>
#include <llvm/Object/ArchiveWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <iostream>
#include <sstream>

// #define __WIN32__

#ifdef __WIN32__
LLD_HAS_DRIVER(coff)
#else
LLD_HAS_DRIVER(elf)
#endif

bool Linker::link(const std::filesystem::path &obj_file, const std::filesystem::path &output_file, const bool is_static) {
    std::vector<const char *> args;
#ifdef __WIN32__
    // Windows COFF linking arguments
    std::string output_exe = output_file.string() + ".exe";
    std::string obj_file_str = obj_file.string();

    args.push_back("lld-link");
    const std::string builtins_link_flag = "-L" + (Generator::get_flintc_cache_path()).string();
    args.push_back(builtins_link_flag.c_str());
    args.push_back("-lbuiltins");
    args.push_back(obj_file_str.c_str());

    std::string out_arg = "/OUT:" + output_exe;
    args.push_back(out_arg.c_str());

    // Get the lib environment variable
    std::vector<std::string> lib_paths;
    const char *lib_env = std::getenv("LIB");

    if (lib_env != nullptr) {
        std::string lib_str(lib_env);

        if (DEBUG_MODE) {
            std::cout << YELLOW << "[Debug Info] " << DEFAULT << "Found LIB environment variable: " << lib_str << std::endl;
        }

        // Split by semicolons
        std::stringstream ss(lib_str);
        std::string path;
        while (std::getline(ss, path, ';')) {
            if (!path.empty()) {
                lib_paths.push_back("/LIBPATH:\"" + path + "\"");
            }
        }
    } else {
        if (DEBUG_MODE) {
            std::cout << YELLOW << "[Debug Info] " << DEFAULT << "Lib environment variable not found, trying system call..." << std::endl;
        }

// Fallback: try to get it via system call
#ifdef _MSC_VER
        FILE *pipe = _popen("cmd /c echo %LIB%", "r");
#else
        FILE *pipe = popen("cmd /c echo %LIB%", "r");
#endif
        if (pipe) {
            char buffer[4096];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string lib_str(buffer);

                // Remove trailing newline if present
                if (!lib_str.empty() && lib_str.back() == '\n') {
                    lib_str.pop_back();
                }

                if (DEBUG_MODE) {
                    std::cout << YELLOW << "[Debug Info] " << DEFAULT << "Got LIB paths via system call: " << lib_str << std::endl;
                }

                // Split by semicolons
                std::stringstream ss(lib_str);
                std::string path;
                while (std::getline(ss, path, ';')) {
                    if (!path.empty()) {
                        lib_paths.push_back(path);
                    }
                }
            }
#ifdef _MSC_VER
            _pclose(pipe);
#else
            pclose(pipe);
#endif
        }
    }

    // Add all library paths to lld
    for (const auto &path : lib_paths) {
        args.push_back(path.c_str());
    }

    // Universal C Runtime
    args.push_back("legacy_stdio_definitions.lib");
    args.push_back("ucrt.lib");
    args.push_back("vcruntime.lib");

    if (is_static) {
        args.push_back("/DEFAULTLIB:libcmt.lib");
        args.push_back("/NODEFAULTLIB:msvcrt.lib");
    } else {
        args.push_back("/DEFAULTLIB:msvcrt.lib");
    }
    args.push_back("/SUBSYSTEM:CONSOLE");

    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] " << DEFAULT << (is_static ? "Static" : "Dynamic")
                  << " Windows linking with arguments:" << std::endl;
        for (const auto &arg : args) {
            std::cout << "  " << arg << std::endl;
        }
    }

    bool result = lld::coff::link(args, llvm::outs(), llvm::errs(), false, false);
    return result;
#else
    // Unix ELF linking arguments
    args.push_back("ld.lld");

    std::vector<std::string> args_vec;
    if (is_static) {
        // For static builds with musl
        args_vec.push_back("-static");

        // Find musl libc.a - check multiple possible locations
        std::vector<std::string> possible_musl_paths = {
            "/usr/lib/musl/lib/libc.a",          // Arch Linux
            "/usr/lib/x86_64-linux-musl/libc.a", // Debian/Ubuntu
            "/lib/x86_64-linux-musl/libc.a",     // Another possible location
            "/usr/lib/libc.a"                    // If musl is the system libc
        };

        std::string_view musl_libc_path;
        for (const auto &path : possible_musl_paths) {
            if (std::filesystem::exists(path)) {
                musl_libc_path = path;
                break;
            }
        }

        if (musl_libc_path.empty()) {
            std::cerr << "Error: Could not find musl libc.a. Please install musl-dev or equivalent." << std::endl;
            return false;
        }

        if (DEBUG_MODE) {
            std::cout << "-- Using musl libc from: " << musl_libc_path << "\n" << std::endl;
        }

        // Find musl's crt1.o (startup file)
        std::string musl_dir = std::filesystem::path(musl_libc_path).parent_path().string();
        std::string musl_crt1 = musl_dir + "/crt1.o";

        if (std::filesystem::exists(musl_crt1)) {
            args_vec.push_back(std::string(musl_crt1).c_str());
        } else {
            // Fall back to system crt1.o
            args_vec.push_back("/usr/lib/crt1.o");
        }

        // Add object file
        args_vec.push_back(obj_file.string().c_str());

        // Use musl libc.a directly by path (not with -l flag)
        args_vec.push_back(std::string(musl_libc_path).c_str());

        for (auto &arg : args_vec) {
            args.emplace_back(arg.c_str());
        }
    } else {
        // For dynamic builds, use regular glibc
        args.push_back("--allow-multiple-definition");
        args.push_back("--no-gc-sections"); // Prevent removal of unused sections
        args.push_back("--no-relax");       // Disable relocation relaxation
        args.push_back("/usr/lib/crt1.o");
        args.push_back("/usr/lib/crti.o");
        args.push_back(obj_file.string().c_str());
        args_vec.push_back("-L" + Generator::get_flintc_cache_path().string());
        args.push_back(args_vec.back().c_str());
        args.push_back("-lbuiltins");
        args.push_back("-L/usr/lib");
        args.push_back("-L/usr/lib/x86_64-linux-gnu");
        args.push_back("-lc");
        args.push_back("/usr/lib/crtn.o");
        args.push_back("--dynamic-linker=/lib64/ld-linux-x86-64.so.2");
    }

    // Output file
    args.push_back("-o");
    args.push_back(output_file.string().c_str());

    if (DEBUG_MODE) {
        std::cout << "-- " << (is_static ? "Static (musl) " : "Dynamic ") << "ELF linking with arguments:" << std::endl;
        for (const auto &arg : args) {
            std::cout << "  " << arg << "\n";
        }
        std::cout << std::endl;
    }

    return lld::elf::link(args, llvm::outs(), llvm::errs(), false, false);
#endif
}

bool Linker::create_static_library(const std::vector<std::filesystem::path> &obj_files, const std::filesystem::path &output_file) {
    // Create archive members from object files
    std::vector<llvm::NewArchiveMember> newMembers;

    for (const auto &obj_file : obj_files) {
        // Create archive member from file
        auto memberOrErr = llvm::NewArchiveMember::getFile(obj_file.string(), /*Deterministic=*/true);

        if (!memberOrErr) {
            std::cerr << "Error: Unable to create archive member from " << obj_file << llvm::toString(memberOrErr.takeError()) << std::endl;
            return false;
        }

        newMembers.push_back(std::move(*memberOrErr));
    }

    // Write the archive file
    llvm::Error err = llvm::writeArchive(output_file.string(), newMembers,
        /*WriteSymtab=*/llvm::SymtabWritingMode::NormalSymtab,
        /*Kind=*/llvm::object::Archive::K_GNU,
        /*Deterministic=*/true,
        /*Thin=*/false, /*OldArchiveBuf=*/nullptr);

    if (err) {
        std::cerr << "Error: Failed to write archive: " << llvm::toString(std::move(err)) << std::endl;
        return false;
    }

    return true;
}
