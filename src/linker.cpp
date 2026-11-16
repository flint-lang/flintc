#include "linker/linker.hpp"
#include "colors.hpp"
#include "globals.hpp"
#include "parser/parser.hpp"

#include <filesystem>
#include <lld/Common/Driver.h>
#include <llvm/Object/ArchiveWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <ostream>

// #define _WIN32

#ifdef _WIN32
#include "cli_parser_base.hpp"
#include "colors.hpp"
#include "profiler.hpp"
#include <cstdlib>
#include <fstream>
#endif

LLD_HAS_DRIVER(coff)
LLD_HAS_DRIVER(elf)
LLD_HAS_DRIVER(mingw)

bool Linker::link(                                       //
    const std::vector<std::filesystem::path> &obj_files, //
    const std::filesystem::path &output_file,            //
    const std::vector<std::string> &flags,               //
    const bool is_static                                 //
) {
    const std::filesystem::path target_path = Parser::get_target_path();

    if (target_path.empty() || !std::filesystem::exists(target_path)) {
        std::cerr << RED << "Linking Error" << DEFAULT << ": The target '" << BLUE << target_triple() << DEFAULT
                  << "' not installed on your system\n";
        std::cerr << "               To install this target, run '" << GREEN << "flint target install " << target_triple() << DEFAULT
                  << "'\n";
        std::cerr << "               " << YELLOW << "NOTE" << DEFAULT
                  << ": Every Flint version newer or equal to 'v0.4.3-core' depends on the 'flint' toolchain to be installed.\n";
        std::cerr
            << "                     This new toolchain also manages the compiler installations. Please uninstall every Flint package\n";
        std::cerr << "                     on your system and only install the 'flint' toolchain package.\n";
        std::flush(std::cerr);
        return false;
    }

    const std::filesystem::path cache_path = Parser::get_flintc_cache_path();

    std::vector<std::string> args;

    switch (COMPILATION_TARGET) {
        case Target::x86_64_linux_gnu: {
            args.push_back("ld.lld");

            args.push_back("--allow-multiple-definition");
            args.push_back("--gc-sections");
            args.push_back("--no-relax");
            args.push_back("-g");

            args.push_back("--sysroot=" + std::filesystem::absolute(target_path).string());
            args.push_back("-L" + target_path.string());
            args.push_back("-L" + cache_path.string());

            if (is_static) {
                args.push_back("--static");
            } else {
                args.push_back("-Bdynamic");
                args.push_back("--dynamic-linker=/lib64/ld-linux-x86-64.so.2");
            }

            // Startup objects
            args.push_back((target_path / "crt1.o").string());
            args.push_back((target_path / "crti.o").string());

            // User objects
            for (const auto &obj_file : obj_files) {
                args.push_back(obj_file.string());
            }

            // Runtime libraries
            args.push_back("--start-group");
            args.push_back("-lbuiltins");
            args.push_back("-lc");
            args.push_back("-lm");
            if (is_static) {
                args.push_back("-lgcc");
                args.push_back("-lgcc_eh");
            }
            args.push_back("--end-group");

            // Final startup object
            args.push_back((target_path / "crtn.o").string());

            args.push_back("-o");
            args.push_back(output_file.string());

            break;
        }
        case Target::x86_64_linux_musl: {
            args.push_back("ld.lld");

            args.push_back("--allow-multiple-definition");
            args.push_back("--gc-sections");
            args.push_back("--no-relax");
            args.push_back("-g");

            args.push_back("-L" + target_path.string());
            args.push_back("-L" + cache_path.string());

            // Startup objects
            args.push_back((target_path / "crt1.o").string());
            args.push_back((target_path / "crti.o").string());

            // Program object files MUST come before the libraries.
            for (const auto &obj_file : obj_files) {
                args.push_back(obj_file.string());
            }

            if (is_static) {
                args.push_back("--static");

                args.push_back("--start-group");
                args.push_back("-lbuiltins");
                args.push_back((target_path / "libc.a").string());
                args.push_back((target_path / "libgcc.a").string());
                args.push_back((target_path / "libgcc_eh.a").string());
                args.push_back("--end-group");
            } else {
                args.push_back("-Bdynamic");
                args.push_back("--dynamic-linker=/lib/ld-musl-x86_64.so.1");
                args.push_back("--no-as-needed");

                // Explicitly select the shared musl library.
                args.push_back("-L" + target_path.string());
                args.push_back("-l:libc.so");

                // GCC runtime remains statically linked.
                args.push_back("-Bstatic");

                args.push_back("--start-group");
                args.push_back("-lbuiltins");
                args.push_back((target_path / "libgcc.a").string());
                args.push_back((target_path / "libgcc_eh.a").string());
                args.push_back("--end-group");
            }

            // Final startup object
            args.push_back((target_path / "crtn.o").string());

            args.push_back("-o");
            args.push_back(output_file.string());
            break;
        }
        case Target::x86_64_windows_gnu:
            args.push_back("ld.lld");

            args.push_back("-m");
            args.push_back("i386pep");

            args.push_back("-o");
            args.push_back(output_file.string() + ".exe");

            args.push_back("-L" + target_path.string());
            args.push_back("-L" + cache_path.string());

            // MinGW startup objects
            args.push_back((target_path / "crt2.o").string());
            args.push_back((target_path / "crtbegin.o").string());
            for (const auto &obj_file : obj_files) {
                args.push_back(obj_file.string());
            }

            args.push_back("-lbuiltins");

            if (is_static) {
                args.push_back("-static");
            } else {
                args.push_back("-Bdynamic");
            }

            // MinGW runtime and Windows API libraries
            args.push_back("--start-group");

            args.push_back("-lmingw32");
            args.push_back("-lgcc");
            args.push_back("-lgcc_eh");
            args.push_back("-lmoldname");
            args.push_back("-lmingwex");
            args.push_back("-lmsvcrt");

            args.push_back("-ladvapi32");
            args.push_back("-lshell32");
            args.push_back("-luser32");
            args.push_back("-lkernel32");

            args.push_back("--end-group");

            args.push_back((target_path / "crtend.o").string());
            break;
        case Target::x86_64_windows_msvc:
            args.push_back("lld-link");
            args.push_back("/lldignoreenv");
            args.push_back("/MACHINE:X64");
            args.push_back("/OUT:" + output_file.string() + ".exe");
            args.push_back("/DEBUG");
            args.push_back("/PDB:" + output_file.string() + ".pdb");
            args.push_back("/IGNORE:4099");
            args.push_back("/SUBSYSTEM:CONSOLE");

            args.push_back("/LIBPATH:" + target_path.string());
            args.push_back("/LIBPATH:" + cache_path.string());

            // Flint builtins and Windows API
            args.push_back("builtins.lib");
            args.push_back("kernel32.lib");
            args.push_back("oldnames.lib");
            args.push_back("legacy_stdio_definitions.lib");
            args.push_back("legacy_stdio_wide_specifiers.lib");

            for (const auto &obj_file : obj_files) {
                args.push_back(obj_file.string());
            }

            if (is_static) {
                // Static CRT (/MT)
                args.push_back("libcmt.lib");
                args.push_back("libvcruntime.lib");
                args.push_back("libucrt.lib");

                // Include if linking against the MSVC C++ standard library.
                args.push_back("libcpmt.lib");
            } else {
                // Dynamic CRT (/MD)
                args.push_back("msvcrt.lib");
                args.push_back("vcruntime.lib");
                args.push_back("ucrt.lib");

                // Include if linking against the MSVC C++ standard library.
                args.push_back("msvcprt.lib");
            }
            break;
    }

    std::vector<const char *> c_args;
    for (const auto &arg : args) {
        c_args.push_back(arg.c_str());
    }
    for (const auto &flag : flags) {
        c_args.push_back(flag.c_str());
    }
    if (PRINT_LINK) {
        std::cout << YELLOW << "[Debug Info] Linking for target '" << target_triple() << "' with arguments:" << DEFAULT << std::endl;
        for (const auto *arg : c_args) {
            std::cout << "  " << arg << '\n';
        }
    }
    switch (COMPILATION_TARGET) {
        case Target::x86_64_linux_gnu:
            [[fallthrough]];
        case Target::x86_64_linux_musl:
            return lld::elf::link(c_args, llvm::outs(), llvm::errs(), false, false);
        case Target::x86_64_windows_gnu:
            return lld::mingw::link(c_args, llvm::outs(), llvm::errs(), false, false);
        case Target::x86_64_windows_msvc:
            return lld::coff::link(c_args, llvm::outs(), llvm::errs(), false, false);
    }
    UNREACHABLE();
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
    std::string file_ending = "";
    switch (COMPILATION_TARGET) {
        case Target::x86_64_linux_gnu:
            [[fallthrough]];
        case Target::x86_64_linux_musl:
            file_ending = ".a";
            break;
        case Target::x86_64_windows_gnu:
            [[fallthrough]];
        case Target::x86_64_windows_msvc:
            file_ending = ".lib";
            break;
    }
    llvm::Error err = llvm::writeArchive(output_file.string() + file_ending, newMembers,
        llvm::SymtabWritingMode::NormalSymtab, // WriteSymtab
        llvm::object::Archive::K_GNU,          // Kind
        true,                                  // Deterministic
        false,                                 // Thin
        nullptr                                // OldArchiveBuf
    );

    if (err) {
        std::cerr << "Error: Failed to write archive: " << llvm::toString(std::move(err)) << std::endl;
        return false;
    }

    return true;
}
