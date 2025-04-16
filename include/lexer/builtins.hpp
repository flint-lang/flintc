#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

/// @enum `BuiltinFunction`
/// @brief An enum describing the builtin function without saying anything about their respective parameter and return types
enum BuiltinFunction { //
    PRINT,
    PRINT_ERR,
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
    // assertions
    // {"assert", ASSERT},         //
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
};

/// @typedef `type_list`
/// @brief This type is used for a list of types in the `builtin_function_types` map to make its signature much clearer
using type_list = std::vector<std::string_view>;

/// @var `builtin_function_types`
/// @brief Map containing all argument and return types and all overloads of builtin functions
static inline std::unordered_map<BuiltinFunction, std::vector<std::pair<type_list, type_list>>> builtin_function_types = {
    {BuiltinFunction::PRINT,
        {
            {{"i32"}, {"void"}},
            {{"i64"}, {"void"}},
            {{"u32"}, {"void"}},
            {{"u64"}, {"void"}},
            {{"f32"}, {"void"}},
            {{"f64"}, {"void"}},
            {{"char"}, {"void"}},
            {{"str"}, {"void"}},
            {{"bool"}, {"void"}},
        }},
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
    {"i32", {"str", "u32", "u64", "i64", "f32", "f64"}},
    {"i64", {"str"}},
    {"u32", {"str", "i32", "i64", "u64", "f32", "f64"}},
    {"u64", {"str"}},
    {"f32", {"str", "f64"}},
    {"f64", {"str"}},
    {"bool", {"str"}},
};
