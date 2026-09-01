#pragma once

#include "assert.hpp"

#include <filesystem>

#ifdef DEBUG_BUILD
constexpr inline bool DEBUG_MODE = true;
#else
constexpr inline bool DEBUG_MODE = false;
#endif

#ifndef VERSION
#define VERSION "unknown"
#endif

#ifndef COMMIT_HASH
#define COMMIT_HASH "unknown"
#endif

#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif

extern bool FIP_ENABLED;
extern bool PRINT_LINES;
extern bool PRINT_TOKENS;
extern bool PRINT_DEPS;
extern bool PRINT_BODY_TOKENS;
extern bool PRINT_AST;
extern bool PRINT_PERFORMANCE;
extern bool PRINT_PROFILE;
extern bool PRINT_TARGET;
extern bool PRINT_LINK;
extern bool PRINT_TYPES;
extern bool PRINT_GARBAGE;
extern bool PRINT_CODEGEN;
extern bool PRINT_IR;
extern bool PRINT_IR_OPTIMIZED;
extern bool PRINT_IR_FILE;
extern bool PRINT_CUMULATIVE_PROFILE_RESULTS;
extern bool HARD_CRASH;
extern bool NO_BINARY;
extern bool NO_GENERATION;

enum class BuiltinLibrary : unsigned int {
    PRINT = 1,
    STR = 2,
    CAST = 4,
    ARITHMETIC = 8,
    ARRAY = 16,
    READ = 32,
    ASSERT = 64,
    FILESYSTEM = 128,
    ENV = 256,
    SYSTEM = 512,
    MATH = 1024,
    PARSE = 2048,
    TIME = 4096,
    DIMA = 8192,
};
extern unsigned int BUILTIN_LIBS_TO_PRINT;

enum class OptimizeMode {
    DEBUG,
    FAST,
};
extern OptimizeMode OPTIMIZE_MODE;

enum class Target {
    NATIVE,
    LINUX,
    WINDOWS_MSVC,
    WINDOWS_GNU,
};
extern Target COMPILATION_TARGET;

inline bool is_target_windows() {
    switch (COMPILATION_TARGET) {
        case Target::WINDOWS_MSVC:
            [[fallthrough]];
        case Target::WINDOWS_GNU:
            return true;
        case Target::LINUX:
            return false;
        case Target::NATIVE:
#ifdef _WIN32
            return true;
#else
            return false;
#endif
    }
    UNREACHABLE();
}

inline const char *target_name() {
    switch (COMPILATION_TARGET) {
        case Target::NATIVE:
            return "native";
        case Target::LINUX:
            return "linux";
        case Target::WINDOWS_MSVC:
            return "windows-msvc";
        case Target::WINDOWS_GNU:
            return "windows-gnu";
    }
    UNREACHABLE();
}

enum class ArithmeticOverflowMode : unsigned int { PRINT = 0, SILENT = 1, CRASH = 2, UNSAFE = 3 };
extern ArithmeticOverflowMode overflow_mode;

enum class ArrayOutOfBoundsMode : unsigned int { PRINT = 0, SILENT = 1, CRASH = 2, UNSAFE = 3 };
extern ArrayOutOfBoundsMode oob_mode;

enum class OpaqueLeakMode : unsigned int { PRINT = 0, SILENT = 1, CRASH = 2 };
extern OpaqueLeakMode opaque_leak_mode;

enum class OptionalUnwrapMode : unsigned int { CRASH = 0, UNSAFE = 1 };
extern OptionalUnwrapMode opt_unwrap_mode;

enum class VariantUnwrapMode : unsigned int { CRASH = 0, UNSAFE = 1 };
extern VariantUnwrapMode var_unwrap_mode;

extern std::filesystem::path main_file_path;
