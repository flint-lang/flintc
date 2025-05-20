#pragma once

#ifdef DEBUG_BUILD
constexpr inline bool DEBUG_MODE = true;
#else
constexpr inline bool DEBUG_MODE = false;
#endif

extern bool PRINT_TOK_STREAM;
extern bool PRINT_DEP_TREE;
extern bool PRINT_AST;
extern bool PRINT_IR_PROGRAM;
extern bool PRINT_PROFILE_RESULTS;
extern bool HARD_CRASH;
extern bool NO_GENERATION;

enum class BuiltinLibrary : unsigned int { PRINT = 1, STR = 2, CAST = 4, ARITHMETIC = 8, ARRAY = 16, READ = 32, ASSERT = 64 };
extern unsigned int BUILTIN_LIBS_TO_PRINT;

enum class ArithmeticOverflowMode : unsigned int { PRINT = 0, SILENT = 1, CRASH = 2, UNSAFE = 3 };
extern ArithmeticOverflowMode overflow_mode;

enum class ArrayOutOfBoundsMode : unsigned int { PRINT = 0, SILENT = 1, CRASH = 2, UNSAFE = 3 };
extern ArrayOutOfBoundsMode oob_mode;
