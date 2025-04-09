#include "linker_interface.hpp"
#include "colors.hpp"
#include "globals.hpp"

#include <iostream>
#include <lld/Common/Driver.h>
#include <llvm/Support/raw_ostream.h>

#if defined(__WIN32__)
LLD_HAS_DRIVER(coff)
#else
LLD_HAS_DRIVER(elf)
#endif

bool LinkerInterface::link(const std::filesystem::path &obj_file, const std::filesystem::path &output_file, const bool is_static) {
    std::vector<const char *> args;
#if defined(_WIN32)
    // Windows COFF linking arguments
    std::string output_exe = output_file.string() + ".exe";

    args.push_back("lld-link");
    args.push_back(obj_file.string().c_str());

    std::string out_arg = "/OUT:" + output_exe;
    args.push_back(out_arg.c_str());

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

    return lld::coff::link(args, llvm::outs(), llvm::errs(), false, false);
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
        args.push_back("/usr/lib/crt1.o");
        args.push_back("/usr/lib/crti.o");
        args.push_back(obj_file.string().c_str());
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
