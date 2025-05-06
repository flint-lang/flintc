#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

/// @enum `BuiltinFunction`
/// @brief An enum describing the builtin function without saying anything about their respective parameter and return types
enum BuiltinFunction {
    // Print functions
    PRINT,
    PRINT_ERR,
    // Read functions
    READ_STR,
    READ_I32,
    READ_I64,
    READ_U32,
    READ_U64,
    READ_F32,
    READ_F64,
    // Assertion functions
    // ASSERT,
    ASSERT_ARG,
    RUN_ON_ALL,
    MAP_ON_ALL,
    FILTER_ON_ALL,
    REDUCE_ON_ALL,
    REDUCE_ON_PAIRS,
    PARTITION_ON_ALL,
    SPLIT_ON_ALL,
};

/// @var `builtin_functions`
/// @brief A map mapping the name of a given builtin function (its string name) to the enum type of the builtin function
static const std::unordered_map<std::string_view, BuiltinFunction> builtin_functions = {
    // printing
    {"print", PRINT},        //
    {"printerr", PRINT_ERR}, //
    // reading from stdin
    {"read_str", READ_STR}, //
    {"read_i32", READ_I32}, //
    {"read_i64", READ_I64}, //
    {"read_u32", READ_U32}, //
    {"read_u64", READ_U64}, //
    {"read_f32", READ_F32}, //
    {"read_f64", READ_F64}, //
    // assertions
    // {"assert", ASSERT},      //
    {"assert_arg", ASSERT_ARG}, //
    // concurrency
    {"run_on_all", RUN_ON_ALL},            //
    {"map_on_all", MAP_ON_ALL},            //
    {"filter_on_all", FILTER_ON_ALL},      //
    {"reduce_on_all", REDUCE_ON_ALL},      //
    {"reduce_on_pairs", REDUCE_ON_PAIRS},  //
    {"partition_on_all", PARTITION_ON_ALL} //
};

/// @enum `CFunctions`
/// @brief An enum used to access all the C functions
enum CFunction {
    MALLOC,
    FREE,
    MEMCPY,
    REALLOC,
    SNPRINTF,
    MEMCMP,
    EXIT,
    ABORT,
    FGETC,
    MEMMOVE,
    STRTOL,
    STRTOUL,
    STRTOF,
    STRTOD,
};

/// @typedef `type_list`
/// @brief This type is used for a list of types in the `builtin_function_types` map to make its signature much clearer
using type_list = std::vector<std::string_view>;

/// @var `builtin_function_types`
/// @brief Map containing all argument and return types and all overloads of builtin functions
static inline std::unordered_map<BuiltinFunction, std::vector<std::pair<type_list, type_list>>> builtin_function_types = {
    {BuiltinFunction::PRINT,
        {
            {{"i32"}, {"void"}},  // Overloaded print function to print i32 values
            {{"i64"}, {"void"}},  // Overloaded print function to print i64 values
            {{"u32"}, {"void"}},  // Overloaded print function to print u32 values
            {{"u64"}, {"void"}},  // Overloaded print function to print u64 values
            {{"f32"}, {"void"}},  // Overloaded print function to print f32 values
            {{"f64"}, {"void"}},  // Overloaded print function to print f64 values
            {{"char"}, {"void"}}, // Overloaded print function to print char values
            {{"str"}, {"void"}},  // Overloaded print function to print str values
            {{"bool"}, {"void"}}, // Overloaded print function to print bool values
        }},
    {BuiltinFunction::READ_STR, {{{}, {"str"}}}},
    {BuiltinFunction::READ_I32, {{{}, {"i32"}}}},
    {BuiltinFunction::READ_I64, {{{}, {"i64"}}}},
    {BuiltinFunction::READ_U32, {{{}, {"u32"}}}},
    {BuiltinFunction::READ_U64, {{{}, {"u64"}}}},
    {BuiltinFunction::READ_F32, {{{}, {"f32"}}}},
    {BuiltinFunction::READ_F64, {{{}, {"f64"}}}},
};

static inline std::unordered_map<std::string_view, std::vector<std::string_view>> primitive_casting_table = {
    {"i32", {"str", "i64", "f32", "f64", "u32", "u64"}},
    {"i64", {"str", "i32", "f32", "f64", "u32", "u64"}},
    {"u32", {"str", "i32", "i64", "f32", "f64", "u64"}},
    {"u64", {"str", "i32", "i64", "f32", "f64", "u32"}},
    {"f32", {"str", "i32", "i64", "f64", "u32", "u64"}},
    {"f64", {"str", "i32", "i64", "f32", "u32", "u64"}},
    {"bool", {"str"}},
};

static inline std::unordered_map<std::string_view, std::vector<std::string_view>> primitive_implicit_casting_table = {
    {"__flint_type_str_lit", {"str"}},
    {"i32", {"str", "u32", "u64", "i64", "f32", "f64", "i32x2", "i32x3", "i32x4", "i32x8"}},
    {"i64", {"str", "i64x2", "i64x3", "i64x4"}},
    {"u32", {"str", "i32", "i64", "u64", "f32", "f64"}},
    {"u64", {"str"}},
    {"f32", {"str", "f64", "f32x2", "f32x3", "f32x4", "f32x8"}},
    {"f64", {"str", "f64x2", "f64x3", "f64x4"}},
    {"bool", {"str", "bool8"}},
    {"char", {"bool8"}},
    {"bool8", {"char"}},
    {"(i32, i32)", {"i32x2"}},
    {"(i32, i32, i32)", {"i32x3"}},
    {"(i32, i32, i32, i32)", {"i32x4"}},
    {"(i64, i64)", {"i64x2"}},
    {"(i64, i64, i64)", {"i64x3"}},
    {"(i64, i64, i64, i64)", {"i64x4"}},
    {"(f32, f32)", {"f32x2"}},
    {"(f32, f32, f32)", {"f32x3"}},
    {"(f32, f32, f32, f32)", {"f32x4"}},
    {"(f64, f64)", {"f64x2"}},
    {"(f64, f64, f64)", {"f64x3"}},
    {"(f64, f64, f64, f64)", {"f64x4"}},
};
