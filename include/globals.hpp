#pragma once

#ifdef DEBUG_BUILD
constexpr inline bool DEBUG_MODE = true;
#else
constexpr inline bool DEBUG_MODE = false;
#endif

extern bool PRINT_TOK_STREAM;
extern bool PRINT_DEP_TREE;
extern bool PRINT_AST;
extern bool PRINT_IR;
extern bool PRINT_PROFILE_RESULTS;
extern bool HARD_CRASH;

enum class ArithmeticOverflowMode { PRINT, SILENT, CRASH, UNSAFE };
extern ArithmeticOverflowMode overflow_mode;
