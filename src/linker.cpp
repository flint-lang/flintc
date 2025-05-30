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
#include "cli_parser_base.hpp"
#include "colors.hpp"
#include "profiler.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>

LLD_HAS_DRIVER(coff)

static const char *fetch_crt_bat_content = R"(@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fetch_crt.ps1" %*
)";

static const char *fetch_crt_ps1_content = R"DELIM($Destination = Join-Path $Env:LocalAppData 'Flint\Cache\flintc\crt'
$x86path = "$Env:ProgramFiles (x86)"

# Ensure output folder exists
New-Item -ItemType Directory -Path $Destination -Force | Out-Null

# Paths based on environment variables
$vsBuildToolsRoot = Join-Path $x86path 'Microsoft Visual Studio\\2022\\BuildTools'
$installer         = Join-Path $Destination      'vs_BuildTools.exe'

# 1) Check if MSVC tools are already installed
$msvcInstallDir = Join-Path $vsBuildToolsRoot 'VC\Tools\MSVC'
if (Test-Path $msvcInstallDir) {
    Write-Host "MSVC toolset already installed at $msvcInstallDir, skipping download and install."
} else {
    # 2) Download VS Build Tools bootstrapper if missing
    if (-Not (Test-Path $installer)) {
        Invoke-WebRequest \
          -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' \
          -OutFile $installer
    }

    # 3) Install only VCTools + MSVC toolset + UCRT headers & libs
    Start-Process $installer -Wait -NoNewWindow -ArgumentList @(
      '--quiet','--wait','--norestart','--nocache',
      '--add','Microsoft.VisualStudio.Workload.VCTools',
      '--add','Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
      '--add','Microsoft.VisualStudio.Component.Windows10SDK.UCRTHeadersAndLibraries'
    )
}

# 4) Copy the libraries we need from MSVC, UCRT and UM

# MSVC import-libs folder
$msvcLibRoot = Join-Path $vsBuildToolsRoot 'VC\Tools\MSVC'
Get-ChildItem "$msvcLibRoot\*\lib\x64\*" `
  -Include msvcrt.lib, vcruntime.lib, libvcruntime.lib, libcmt.lib, legacy_stdio_definitions.lib, legacy_stdio_wide_specifiers.lib, kernel32.lib `
  -Recurse |
  Copy-Item -Destination $Destination -Force

# UCRT import-lib folder
$ucrtLibRoot = Join-Path $x86path 'Windows Kits\10\Lib'
Get-ChildItem "$ucrtLibRoot\*\ucrt\x64" `
  -Include ucrt.lib `
  -Recurse |
  Copy-Item -Destination $Destination -Force

# UM import-lib folder
$ucrtLibRoot = Join-Path $x86path 'Windows Kits\10\Lib'
Get-ChildItem "$ucrtLibRoot\*\um\x64" `
  -Include kernel32.lib `
  -Recurse |
  Copy-Item -Destination $Destination -Force

Write-Host "All .lib files have been placed in $Destination"

# 5) Remove the 'vs_BuildTools.exe' file
if (Test-Path $installer) {
    Remove-Item $installer -Force
    Write-Host "'vs_BuildTools.exe' has been removed."
}
)DELIM";
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
    args.push_back("/DEBUG");
    std::string pdb_flag = "/PDB:" + output_file.string() + ".pdb";
    args.push_back(pdb_flag.c_str());
    args.push_back("/SUBSYSTEM:CONSOLE");
    args.push_back("/NODEFAULTLIB:msvcrt.lib");

    // Get the cache path
    std::filesystem::path cache_path = Generator::get_flintc_cache_path();

    // Check if the crt path exists in the cache path, and if it exists whether all the libraries we need are present in it
    std::filesystem::path crt_path = cache_path / "crt";
    bool crt_libs_present = std::filesystem::exists(crt_path);
    if (crt_libs_present) {
        crt_libs_present = crt_libs_present && std::filesystem::exists(crt_path / "kernel32.lib");
        crt_libs_present = crt_libs_present && std::filesystem::exists(crt_path / "legacy_stdio_definitions.lib");
        crt_libs_present = crt_libs_present && std::filesystem::exists(crt_path / "legacy_stdio_wide_specifiers.lib");
        crt_libs_present = crt_libs_present && std::filesystem::exists(crt_path / "libcmt.lib");
        crt_libs_present = crt_libs_present && std::filesystem::exists(crt_path / "libvcruntime.lib");
        crt_libs_present = crt_libs_present && std::filesystem::exists(crt_path / "msvcrt.lib");
        crt_libs_present = crt_libs_present && std::filesystem::exists(crt_path / "ucrt.lib");
        crt_libs_present = crt_libs_present && std::filesystem::exists(crt_path / "vcruntime.lib");
    }
    if (!crt_libs_present) {
        // One or more lib is missing, call the bash script
        if (DEBUG_MODE) {
            std::cout << YELLOW << "[Debug Info] " << DEFAULT << "One or more crt libraries are missing" << std::endl;
        }
        Profiler::start_task("Fetching crt libraries");
        std::filesystem::create_directories(crt_path);
        std::filesystem::path bat_file = crt_path / "fetch_crt.bat";
        if (!std::filesystem::exists(bat_file)) {
            std::ofstream ofs(bat_file, std::ios::binary);
            ofs << fetch_crt_bat_content;
        }
        std::filesystem::path ps1_file = crt_path / "fetch_crt.ps1";
        if (!std::filesystem::exists(ps1_file)) {
            std::ofstream ofs(ps1_file, std::ios::binary);
            ofs << fetch_crt_ps1_content;
        }
        const std::string bat_file_str = bat_file.string();
        const auto [res, output] = CLIParserBase::get_command_output(bat_file_str.c_str());
        Profiler::end_task("Fetching crt libraries");
        if (res != 0) {
            std::cout << RED << "Error: " << DEFAULT << "Fetching the required crt libraries failed! Command output:\n"
                      << output << std::endl;
            return false;
        }
    }

    // Get the lib environment variable
    std::vector<std::string> lib_paths;
    if (cache_path.string().find(' ') == std::string::npos) {
        lib_paths.push_back("/LIBPATH:" + cache_path.string());
        lib_paths.push_back("/LIBPATH:" + crt_path.string());
    } else {
        // Only add the " ... " if the path contains any spaces
        lib_paths.push_back("/LIBPATH:\"" + cache_path.string() + "\"");
        lib_paths.push_back("/LIBPATH:\"" + crt_path.string() + "\"");
    }
    const char *lib_env = std::getenv("LIB");
    std::string lib_env_str = "";

    if (lib_env != nullptr) {
        lib_env_str = std::string(lib_env);
    } else {
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
                if (lib_str == "%LIB%") {
// We are in a power-shell
#ifdef _MSC_VER
                    _pclose(pipe);
                    pipe = _popen("cmd /c echo $Env:LIB", "r");
#else
                    pclose(pipe);
                    pipe = popen("cmd /c echo $Env:LIB", "r");
#endif
                    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                        lib_str = std::string(buffer);
                    }
                }

                // Remove trailing newline if present
                if (!lib_str.empty() && lib_str.back() == '\n') {
                    lib_str.pop_back();
                }
                lib_env_str = lib_str;
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
        args.push_back("-g");
        args_buffer[args_id] = obj_file.string();
        args.push_back(args_buffer[args_id++].c_str());
        args_buffer[args_id] = std::string("-L" + Generator::get_flintc_cache_path().string());
        args.push_back(args_buffer[args_id++].c_str());
        args.push_back("-lbuiltins");
        args.push_back("-L/usr/lib");
        args.push_back("-L/usr/lib/x86_64-linux-gnu");
        args.push_back("-lc");
        args.push_back("-l:crt1.o");
        args.push_back("-l:crti.o");
        args.push_back("-l:crtn.o");
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
