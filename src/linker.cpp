#include "linker/linker.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"

#include <lld/Common/Driver.h>
#include <llvm/Object/ArchiveWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>

// #define __WIN32__

#ifdef __WIN32__
#include "colors.hpp"
#include <cstdlib>
#include <sstream>
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
    args.push_back(obj_file_str.c_str());

    std::string out_arg = "/OUT:" + output_exe;
    args.push_back(out_arg.c_str());
    args.push_back("/VERBOSE:LIB");
    args.push_back("/SUBSYSTEM:CONSOLE");
    args.push_back("/NODEFAULTLIB:msvcrt.lib");

    // Get the lib environment variable
    std::vector<std::string> lib_paths;
    std::string cache_path = Generator::get_flintc_cache_path().string();
    if (cache_path.find(' ') == std::string::npos) {
        lib_paths.push_back("/LIBPATH:" + cache_path);
    } else {
        // Only add the " ... " if the path contains any spaces
        lib_paths.push_back("/LIBPATH:\"" + cache_path + "\"");
    }
    const char *lib_env = std::getenv("LIB");
    std::string lib_env_str(lib_env);

    if (lib_env != nullptr) {
        if (DEBUG_MODE) {
            std::cout << YELLOW << "[Debug Info] Found LIB environment variable: " << DEFAULT << lib_env_str << std::endl;
        }

        // Split by semicolons
        std::stringstream ss(lib_env_str);
        std::string path;
        while (std::getline(ss, path, ';')) {
            std::filesystem::path lib_path(path);
            lib_path = lib_path.parent_path() / "x64";
            if (std::filesystem::exists(lib_path)) {
                lib_paths.push_back("/LIBPATH:\"" + lib_path.string() + "\"");
            }
        }
    } else {
        if (DEBUG_MODE) {
            std::cout << YELLOW << "[Debug Info] Lib environment variable not found, trying system call..." << DEFAULT << std::endl;
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
                    std::cout << YELLOW << "[Debug Info] Got LIB paths via system call: " << DEFAULT << lib_str << std::endl;
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

    // Link against the builtins library
    args.push_back("libbuiltins.lib");
    // Universal C Runtime
    args.push_back("legacy_stdio_definitions.lib");
    if (is_static) {
        args.push_back("libvcruntime.lib");
        args.push_back("ucrt.lib");
        args.push_back("libcmt.lib");
    } else {
        args.push_back("vcruntime.lib");
        args.push_back("ucrt.lib");
        args.push_back("msvcrt.lib");
    }

    // Set the 'LIB' environment variable to nothing so lld-link ignores all of it
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Clearing the 'LIB' environment variable..." << DEFAULT << std::endl;
    }
#ifdef _MSC_VER
    _putenv(const_cast<char *>("LIB="));
#else
    putenv(const_cast<char *>("LIB="));
#endif

    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] " << (is_static ? "Static" : "Dynamic") << " Windows linking with arguments:" << DEFAULT
                  << std::endl;
        for (const auto &arg : args) {
            std::cout << "  " << arg << std::endl;
        }
    }
    bool result = lld::coff::link(args, llvm::outs(), llvm::errs(), false, false);
    // Set the 'LIB' environemnt variable back to what it was originally
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Putting the original content of the 'LIB' environment variable back into it: " << DEFAULT
                  << lib_env_str << std::endl;
    }
    lib_env_str = "LIB=" + lib_env_str;
#ifdef _MSC_VER
    _putenv(const_cast<char *>(lib_env_str.c_str()));
#else
    putenv(const_cast<char *>(lib_env_str.c_str()));
#endif
    return result;
#else
    // Unix ELF linking arguments
    args.push_back("ld.lld");

    std::array<std::string, 30> args_buffer;
    unsigned int args_id = 0;
    if (is_static) {
        // For static builds with musl
        args_buffer[args_id++] = "-static";

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
            args_buffer[args_id++] = musl_crt1;
        } else {
            // Fall back to system crt1.o
            args_buffer[args_id++] = "/usr/lib/crt1.o";
        }

        // Add object file
        args_buffer[args_id++] = obj_file.string();

        // Use musl libc.a directly by path (not with -l flag)
        args_buffer[args_id++] = std::string(musl_libc_path);

        for (unsigned int i = 0; i < args_id; i++) {
            args.emplace_back(args_buffer[i].c_str());
        }
    } else {
        // For dynamic builds, use regular glibc
        args.push_back("--allow-multiple-definition");
        args.push_back("--no-gc-sections"); // Prevent removal of unused sections
        args.push_back("--no-relax");       // Disable relocation relaxation
        args.push_back("/usr/lib/crt1.o");
        args.push_back("/usr/lib/crti.o");
        args_buffer[args_id] = obj_file.string();
        args.push_back(args_buffer[args_id++].c_str());
        args_buffer[args_id] = std::string("-L" + Generator::get_flintc_cache_path().string());
        args.push_back(args_buffer[args_id++].c_str());
        args.push_back("-lbuiltins");
        args.push_back("-L/usr/lib");
        args.push_back("-L/usr/lib/x86_64-linux-gnu");
        args.push_back("-lc");
        args.push_back("/usr/lib/crtn.o");
        args.push_back("--dynamic-linker=/lib64/ld-linux-x86-64.so.2");
    }

    // Output file
    args.push_back("-o");
    args_buffer[args_id] = output_file.string();
    args.push_back(args_buffer[args_id++].c_str());

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
#ifdef __WIN32__
    const std::string file_ending = ".lib";
#else
    const std::string file_ending = ".a";
#endif
    llvm::Error err = llvm::writeArchive(output_file.string() + file_ending, newMembers,
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
