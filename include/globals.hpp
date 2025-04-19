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

extern bool PRINT_IR_PRINT_O;
extern bool PRINT_IR_STR_O;
extern bool PRINT_IR_CAST_O;
extern bool PRINT_IR_ARITHMETIC_O;

enum class ArithmeticOverflowMode : unsigned int { PRINT = 0, SILENT = 1, CRASH = 2, UNSAFE = 3 };
extern ArithmeticOverflowMode overflow_mode;
