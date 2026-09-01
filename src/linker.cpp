#include "linker/linker.hpp"
#include "colors.hpp"
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
        Invoke-WebRequest `
          -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' `
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

static const char *fetch_musl_bat_content = R"(@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fetch_musl.ps1" %*
)";

static const char *fetch_musl_ps1_content = R"DELIM(Param([string]$Destination = "")
if ($Destination -eq "") {
    Write-Host "Error: No destination directory was given."
    exit 1
}

$MuslArchive = Join-Path $Destination 'x86_64-linux-musl-cross.tgz'
$MuslUrl      = 'https://more.musl.cc/x86_64-linux-musl/x86_64-linux-musl-cross.tgz'
$ExpectedSha  = '40ff5f88e28cfbf45264c75cc2d140170d7e508c87d21e05d2932417565ba3b4b9c9d24587d321c126cee74213198206ecd6687c185d1f4a1d632cb6112527fb'

New-Item -ItemType Directory -Path $Destination -Force | Out-Null

# 1) Download the archive if it is not present yet
if (-Not (Test-Path $MuslArchive)) {
    Write-Host "Downloading the musl sysroot..."
    Invoke-WebRequest -Uri $MuslUrl -OutFile $MuslArchive
}

# 2) Verify the integrity of the archive
$Hash = (Get-FileHash -Algorithm SHA512 -Path $MuslArchive).Hash.ToLower()
if ($Hash -ne $ExpectedSha) {
    Write-Host "Error: SHA512 mismatch for '$MuslArchive'."
    Remove-Item $MuslArchive -Force -ErrorAction SilentlyContinue
    exit 1
}

# 3) Extract the files we need out of the tarball
$TarRoot = 'x86_64-linux-musl-cross';
$Files = @(
    @{ From = "$TarRoot/x86_64-linux-musl/lib/libc.a";             To = 'libc.a' },
    @{ From = "$TarRoot/x86_64-linux-musl/lib/crt1.o";             To = 'crt1.o' },
    @{ From = "$TarRoot/x86_64-linux-musl/lib/crti.o";             To = 'crti.o' },
    @{ From = "$TarRoot/x86_64-linux-musl/lib/crtn.o";             To = 'crtn.o' },
    @{ From = "$TarRoot/lib/gcc/x86_64-linux-musl/11.2.1/libgcc.a";    To = 'gcc/libgcc.a' },
    @{ From = "$TarRoot/lib/gcc/x86_64-linux-musl/11.2.1/libgcc_eh.a"; To = 'gcc/libgcc_eh.a' }
)

foreach ($File in $Files) {
    $Target = Join-Path $Destination $File.To
    $Dir = Split-Path $Target -Parent
    New-Item -ItemType Directory -Path $Dir -Force | Out-Null

    # Extract the single file (with its full path) into a scratch dir, then move
    # it to its final flat location. This keeps binary integrity intact on
    # Windows PowerShell (out-file pipelines can corrupt binary data).
    $Scratch = Join-Path $Destination '.scratch'
    New-Item -ItemType Directory -Path $Scratch -Force | Out-Null
    Remove-Item (Join-Path $Scratch $TarRoot) -Recurse -Force -ErrorAction SilentlyContinue
    tar -xzf $MuslArchive -C $Scratch $File.From
    if (-Not (Test-Path (Join-Path $Scratch $File.From))) {
        Write-Host "Error: Failed to extract '$($File.From)'."
        exit 1
    }
    Copy-Item (Join-Path $Scratch $File.From) -Destination $Target -Force
    Remove-Item (Join-Path $Scratch $TarRoot) -Recurse -Force -ErrorAction SilentlyContinue
}

# 4) Remove the archive again
Remove-Item $MuslArchive -Force -ErrorAction SilentlyContinue
Write-Host "musl libc has been placed in $Destination"
)DELIM";
#endif

LLD_HAS_DRIVER(coff)
LLD_HAS_DRIVER(elf)
LLD_HAS_DRIVER(mingw)

bool Linker::link(                                       //
    const std::vector<std::filesystem::path> &obj_files, ///
    const std::filesystem::path &output_file,            //
    const std::vector<std::string> &flags,               //
    const bool is_static                                 //
) {
    switch (COMPILATION_TARGET) {
        case Target::NATIVE:
#ifdef __WIN32__
            return link_windows_msvc(obj_files, output_file, flags, is_static);
#else
            return is_static ? link_linux_musl(obj_files, output_file, flags) : link_linux_gnu(obj_files, output_file, flags);
#endif
            break;
        case Target::LINUX:
#ifdef __WIN32__
            return link_linux_musl(obj_files, output_file, flags);
#else
            return is_static ? link_linux_musl(obj_files, output_file, flags) : link_linux_gnu(obj_files, output_file, flags);
#endif
            break;
        case Target::WINDOWS_MSVC:
            return link_windows_msvc(obj_files, output_file, flags, is_static);
            break;
        case Target::WINDOWS_GNU:
            return link_windows_gnu(obj_files, output_file, flags, is_static);
            break;
    }
    UNREACHABLE();
    return false;
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
        case Target::NATIVE:
#ifdef __WIN32__
            file_ending = ".lib";
#else
            file_ending = ".a";
#endif
            break;
        case Target::LINUX:
            file_ending = ".a";
            break;
        case Target::WINDOWS_MSVC:
            file_ending = ".lib";
            break;
        case Target::WINDOWS_GNU:
            file_ending = ".a";
            break;
    }
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

bool Linker::fetch_crt_libs() {
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
#ifdef __WIN32__
        // One or more lib is missing, call the bash script
        if (DEBUG_MODE) {
            std::cout << YELLOW << "[Debug Info] " << "One or more crt libraries are missing" << DEFAULT << std::endl;
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
#else
        // One or more libs are missing, re-fetch them and put them into the crt path
#endif
    }
    return true;
}

bool Linker::fetch_musl_libs() {
#ifdef __WIN32__
    // Check if the musl libs exist in the cache path. We need the musl libc archive, the crate startup objects and the GCC support
    // libraries to link against when cross-compiling to Linux.
    std::filesystem::path musl_path = Generator::get_flintc_cache_path() / "musl";
    bool musl_libs_present = std::filesystem::exists(musl_path) && std::filesystem::exists(musl_path / "libc.a");
    musl_libs_present = musl_libs_present && std::filesystem::exists(musl_path / "crt1.o");
    musl_libs_present = musl_libs_present && std::filesystem::exists(musl_path / "crti.o");
    musl_libs_present = musl_libs_present && std::filesystem::exists(musl_path / "crtn.o");
    musl_libs_present = musl_libs_present && std::filesystem::exists(musl_path / "gcc" / "libgcc.a");
    musl_libs_present = musl_libs_present && std::filesystem::exists(musl_path / "gcc" / "libgcc_eh.a");
    if (musl_libs_present) {
        return true;
    }
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] " << "One or more musl libraries are missing" << DEFAULT << std::endl;
    }
    Profiler::start_task("Fetching musl libraries");
    std::filesystem::create_directories(musl_path);
    std::filesystem::path bat_file = musl_path / "fetch_musl.bat";
    if (!std::filesystem::exists(bat_file)) {
        std::ofstream ofs(bat_file, std::ios::binary);
        ofs << fetch_musl_bat_content;
    }
    std::filesystem::path ps1_file = musl_path / "fetch_musl.ps1";
    if (!std::filesystem::exists(ps1_file)) {
        std::ofstream ofs(ps1_file, std::ios::binary);
        ofs << fetch_musl_ps1_content;
    }
    // Pass the destination directory to the script so it can place the libs into the (cross-compile) cache directory.
    const std::string destination = "\"" + musl_path.string() + "\"";
    const auto [res, output] = CLIParserBase::get_command_output(bat_file.string() + " " + destination);
    Profiler::end_task("Fetching musl libraries");
    if (res != 0) {
        std::cout << RED << "Error: " << DEFAULT << "Fetching the required musl libraries failed! Command output:\n" << output << std::endl;
        return false;
    }
#else
    // On Linux the musl libraries are discovered from the system directly, so there is nothing to fetch.
#endif
    return true;
}

std::string Linker::get_lib_env_win() {
    // Get the lib environment variable
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
    return lib_env_str;
}

std::optional<std::vector<std::string>> Linker::get_windows_msvc_args( //
    const std::vector<std::filesystem::path> &obj_files,               //
    const std::filesystem::path &output_file,                          //
    const bool is_static                                               //
) {
    std::vector<std::string> args;
    std::string output_exe = output_file.string() + ".exe";

    args.push_back("lld-link");
    for (const auto &obj_file : obj_files) {
        args.push_back(obj_file.string());
    }

    args.push_back("/OUT:" + output_exe);
    args.push_back("/VERBOSE:LIB");
    args.push_back("/DEBUG");
    args.push_back("/PDB:" + output_file.string() + ".pdb");
    args.push_back("/SUBSYSTEM:CONSOLE");
    args.push_back("/NODEFAULTLIB:msvcrt.lib");

    // Get the cache path
    std::filesystem::path cache_path = Generator::get_flintc_cache_path();

    // Fetch the crt libs
    std::filesystem::path crt_path = cache_path / "crt";
    if (!fetch_crt_libs()) {
        return std::nullopt;
    }

    // Get the lib environment variable
    if (cache_path.string().find(' ') == std::string::npos) {
        args.push_back("/LIBPATH:" + cache_path.string());
        args.push_back("/LIBPATH:" + crt_path.string());
    } else {
        // Only add the " ... " if the path contains any spaces
        args.push_back("/LIBPATH:\"" + cache_path.string() + "\"");
        args.push_back("/LIBPATH:\"" + crt_path.string() + "\"");
    }

    // Link against the builtins library
    args.push_back("libbuiltins.lib");
    args.push_back("kernel32.lib");
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
    return args;
}

bool Linker::link_windows_msvc(                          //
    const std::vector<std::filesystem::path> &obj_files, //
    const std::filesystem::path &output_file,            //
    const std::vector<std::string> &flags,               //
    const bool is_static                                 //
) {
#ifdef __WIN32__
    // Get the 'LIB' environment variable
    std::string lib_env_str = get_lib_env_win();

    // Set the 'LIB' environment variable to nothing so lld-link ignores all of it
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Clearing the 'LIB' environment variable..." << DEFAULT << std::endl;
    }
#ifdef _MSC_VER
    _putenv(const_cast<char *>("LIB="));
#else
    putenv(const_cast<char *>("LIB="));
#endif
#endif

    // Get the arguments with which to call the linker
    std::optional<std::vector<std::string>> arguments = get_windows_msvc_args(obj_files, output_file, is_static);
    if (!arguments.has_value()) {
        return false;
    }
    std::vector<const char *> args;
    for (const auto &arg : arguments.value()) {
        args.push_back(arg.c_str());
    }
    for (const auto &flag : flags) {
        args.push_back(flag.c_str());
    }
    if (PRINT_LINK) {
        std::cout << YELLOW << "[Debug Info] " << (is_static ? "Static" : "Dynamic") << " Windows linking with arguments:" << DEFAULT
                  << std::endl;
        for (const auto &arg : args) {
            std::cout << "  " << arg << "\n";
        }
        for (const auto &flag : flags) {
            std::cout << "  " << flag << "\n";
        }
        std::cout << std::endl;
    }
    bool result = lld::coff::link(args, llvm::outs(), llvm::errs(), false, false);

#ifdef __WIN32__
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
#endif
    return result;
}

std::optional<std::vector<std::string>> Linker::get_linux_gnu_args( //
    const std::vector<std::filesystem::path> &obj_files,            //
    const std::filesystem::path &output_file                        //
) {
    std::vector<std::string> args;
    args.push_back("ld.lld");

    // Dynamic builds with regular glibc
    args.push_back("--allow-multiple-definition");
    args.push_back("--gc-sections"); // Prevent removal of unused sections
    args.push_back("--no-relax");    // Disable relocation relaxation
    args.push_back("-g");
    for (const auto &obj_file : obj_files) {
        args.push_back(obj_file.string());
    }
    args.push_back("-L" + Generator::get_flintc_cache_path().string());
    args.push_back("-lbuiltins");
    args.push_back("-L/usr/lib");
    args.push_back("-L/usr/lib/x86_64-linux-gnu");
    args.push_back("-lc");
    args.push_back("-lm");
    args.push_back("-l:crt1.o");
    args.push_back("-l:crti.o");
    args.push_back("-l:crtn.o");
    args.push_back("--dynamic-linker=/lib64/ld-linux-x86-64.so.2");

    // Output file
    args.push_back("-o");
    args.push_back(output_file.string());
    return args;
}

bool Linker::link_linux_gnu(                             //
    const std::vector<std::filesystem::path> &obj_files, //
    const std::filesystem::path &output_file,            //
    const std::vector<std::string> &flags                //
) {
    // Get the arguments for linking
    std::optional<std::vector<std::string>> arguments = get_linux_gnu_args(obj_files, output_file);
    if (!arguments.has_value()) {
        return false;
    }
    if (PRINT_LINK) {
        std::cout << YELLOW << "[Debug Info] " << "Dynamic (glibc) ELF linking with arguments:" << DEFAULT << std::endl;
        for (const auto &arg : arguments.value()) {
            std::cout << "  " << arg << "\n";
        }
        for (const auto &flag : flags) {
            std::cout << "  " << flag << "\n";
        }
        std::cout << std::endl;
    }
    std::vector<const char *> args;
    for (const auto &arg : arguments.value()) {
        args.push_back(arg.c_str());
    }
    for (const auto &flag : flags) {
        args.push_back(flag.c_str());
    }

    return lld::elf::link(args, llvm::outs(), llvm::errs(), false, false);
}

std::optional<std::vector<std::string>> Linker::get_linux_musl_args( //
    const std::vector<std::filesystem::path> &obj_files,             //
    const std::filesystem::path &output_file                         //
) {
    std::vector<std::string> args;
    args.push_back("ld.lld");

#ifdef __WIN32__
    // Cross-compiling to Linux from Windows: glibc is not available, so we
    // statically link against the previously fetched musl libc.
    if (!fetch_musl_libs()) {
        return std::nullopt;
    }
    std::filesystem::path musl_path = Generator::get_flintc_cache_path() / "musl";

    args.push_back("-static");
    args.push_back((musl_path / "crt1.o").string());
    for (const auto &obj_file : obj_files) {
        args.push_back(obj_file.string());
    }
    args.push_back("-L" + Generator::get_flintc_cache_path().string());
    args.push_back("-lbuiltins");
    // Use the musl libc.a directly by path (not with -l flag)
    args.push_back((musl_path / "libc.a").string());
    args.push_back("-L" + (musl_path / "gcc").string());
    args.push_back("-lgcc");
#else
    // For static builds with musl
    args.push_back("-static");
    args.push_back("-L" + Generator::get_flintc_cache_path().string());
    args.push_back("-lbuiltins");

    // Find musl libc.a - check multiple possible locations
    std::vector<std::string> possible_musl_paths = {
        "/usr/lib/musl/lib/libc.a",          // Arch Linux
        "/usr/lib/x86_64-linux-musl/libc.a", // Debian/Ubuntu
        "/lib/x86_64-linux-musl/libc.a",     // Another possible location
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
        return std::nullopt;
    }
    if (DEBUG_MODE) {
        std::cout << "-- Using musl libc from: " << musl_libc_path << "\n" << std::endl;
    }

    // Find musl's crt1.o (startup file)
    std::string musl_dir = std::filesystem::path(musl_libc_path).parent_path().string();
    std::string musl_crt1 = musl_dir + "/crt1.o";

    if (std::filesystem::exists(musl_crt1)) {
        args.push_back(musl_crt1);
    } else {
        // Fall back to system crt1.o
        args.push_back("/usr/lib/crt1.o");
    }

    // Add object files
    for (const auto &obj_file : obj_files) {
        args.push_back(obj_file.string());
    }

    // Use musl libc.a directly by path (not with -l flag)
    args.push_back(std::string(musl_libc_path));
#endif // not __WIN32__

    // Output file
    args.push_back("-o");
    args.push_back(output_file.string());
    return args;
}

bool Linker::link_linux_musl(                            //
    const std::vector<std::filesystem::path> &obj_files, //
    const std::filesystem::path &output_file,            //
    const std::vector<std::string> &flags                //
) {
    // Get the arguments for linking
    std::optional<std::vector<std::string>> arguments = get_linux_musl_args(obj_files, output_file);
    if (!arguments.has_value()) {
        return false;
    }
    if (PRINT_LINK) {
        std::cout << YELLOW << "[Debug Info] " << "Static (musl) ELF linking with arguments:" << DEFAULT << std::endl;
        for (const auto &arg : arguments.value()) {
            std::cout << "  " << arg << "\n";
        }
        for (const auto &flag : flags) {
            std::cout << "  " << flag << "\n";
        }
        std::cout << std::endl;
    }
    std::vector<const char *> args;
    for (const auto &arg : arguments.value()) {
        args.push_back(arg.c_str());
    }
    for (const auto &flag : flags) {
        args.push_back(flag.c_str());
    }

    return lld::elf::link(args, llvm::outs(), llvm::errs(), false, false);
}

std::optional<std::vector<std::string>> Linker::get_windows_gnu_args( //
    const std::vector<std::filesystem::path> &obj_files,              //
    const std::filesystem::path &output_file,                         //
    const bool is_static                                              //
) {
    std::vector<std::string> args;
    args.push_back("ld.lld");

    // MinGW driver target machine (PE COFF, x86_64)
    args.push_back("-m");
    args.push_back("i386pep");
    args.push_back("-o");
    args.push_back(output_file.string() + ".exe");

    // Probe for an installed MinGW sysroot (similar to how musl is discovered on Linux). The mingw-w64 runtime lives under a triple
    // directory, e.g. '/usr/x86_64-w64-mingw32/lib'
    const std::string triple = "x86_64-w64-mingw32";
    const std::filesystem::path lib_dir = std::filesystem::path("/usr") / triple / "lib";
    if (lib_dir.empty()) {
        std::cerr << "Error: Could not find an installed MinGW toolchain." << std::endl;
        return std::nullopt;
    }
    if (!std::filesystem::exists(lib_dir / "crt2.o")) {
        std::cerr << "Error: Could not find file 'crt2.o' in '" << lib_dir.string() << "'" << std::endl;
        return std::nullopt;
    }
    if (!std::filesystem::exists(lib_dir / "libmingw32.a")) {
        std::cerr << "Error: Could not find file 'libmingw32.a' in '" << lib_dir.string() << "'" << std::endl;
        return std::nullopt;
    }

    // Locate the versioned GCC support directory (contains libgcc.a, crtbegin.o, crtend.o).
    const std::filesystem::path gcc_base = std::filesystem::path("/usr") / "lib" / "gcc" / triple;
    std::filesystem::path gcc_dir;
    std::error_code ec;
    if (std::filesystem::exists(gcc_base)) {
        for (const auto &entry : std::filesystem::directory_iterator(gcc_base, ec)) {
            if (entry.is_directory() && std::filesystem::exists(entry.path() / "libgcc.a")) {
                // Pick the highest version directory if several are present.
                if (gcc_dir.empty() || entry.path().filename().string() > gcc_dir.filename().string()) {
                    gcc_dir = entry.path();
                }
            }
        }
    }
    if (gcc_dir.empty()) {
        std::cerr << "Error: Could not find the GCC support directory ('libgcc.a') for the MinGW toolchain in '" << gcc_base.string()
                  << "'." << std::endl;
        return std::nullopt;
    }

    args.push_back("-L" + gcc_dir.string());
    args.push_back("-L" + lib_dir.string());
    args.push_back((lib_dir / "crt2.o").string());
    args.push_back((gcc_dir / "crtbegin.o").string());
    for (const auto &obj_file : obj_files) {
        args.push_back(obj_file.string());
    }

    // Link against the builtins library from the flintc cache
    args.push_back("-L" + Generator::get_flintc_cache_path().string());
    args.push_back("-lbuiltins");
    if (is_static) {
        args.push_back("-static");
    } else {
        args.push_back("-Bdynamic");
    }

    args.push_back("-lmingw32");
    args.push_back("-lgcc");
    args.push_back("-lgcc_eh");
    args.push_back("-lmoldname");
    args.push_back("-lmingwex");
    args.push_back("-lmsvcrt");
    args.push_back("-lkernel32");
    // Statically link winpthread so the produced executable doesn't depend on the 'libwinpthread-1.dll' shared library at runtime.
    args.push_back("-Bstatic");
    args.push_back("-lpthread");
    args.push_back("-Bdynamic");
    args.push_back("-ladvapi32");
    args.push_back("-lshell32");
    args.push_back("-luser32");
    args.push_back("-lkernel32");

    args.push_back((gcc_dir / "crtend.o").string());
    return args;
}

bool Linker::link_windows_gnu(                           //
    const std::vector<std::filesystem::path> &obj_files, //
    const std::filesystem::path &output_file,            //
    const std::vector<std::string> &flags,               //
    const bool is_static                                 //
) {
    // Get the arguments for linking
    std::optional<std::vector<std::string>> arguments = get_windows_gnu_args(obj_files, output_file, is_static);
    if (!arguments.has_value()) {
        return false;
    }
    if (PRINT_LINK) {
        std::cout << YELLOW << "[Debug Info] " << (is_static ? "Static " : "Dynamic ") << "MinGW linking with arguments:" << DEFAULT
                  << std::endl;
        for (const auto &arg : arguments.value()) {
            std::cout << "  " << arg << "\n";
        }
        for (const auto &flag : flags) {
            std::cout << "  " << flag << "\n";
        }
        std::cout << std::endl;
    }
    std::vector<const char *> args;
    for (const auto &arg : arguments.value()) {
        args.push_back(arg.c_str());
    }
    for (const auto &flag : flags) {
        args.push_back(flag.c_str());
    }

    return lld::mingw::link(args, llvm::outs(), llvm::errs(), false, false);
}
