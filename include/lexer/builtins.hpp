#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

/// @typedef `type_list`
/// @brief This type is used for a list of types in the `builtin_function_types` map to make its signature much clearer
using type_list = std::vector<std::string_view>;

/// @typedef `overloads`
/// @brief This type is used for all overloads of a function, it contains the argument and return types of a function
using overloads = std::vector<std::pair<type_list, type_list>>;

/// @typedef `function_list`
/// @brief A function list is a list of functions together with its signature overloads
using function_overload_list = std::unordered_map<std::string_view, overloads>;

/// @var `core_module_functions`
/// @brief A map containing all core modules and maps the module to its functions
static inline std::unordered_map<std::string_view, function_overload_list> core_module_functions = {
    {"print", // The 'print' module
        {
            {"print", // The 'print' function
                {
                    {{"i32"}, {"void"}},  // The 'i32' argument overload of the 'print' function
                    {{"i64"}, {"void"}},  // The 'i64' argument overload of the 'print' function
                    {{"u32"}, {"void"}},  // The 'u32' argument overload of the 'print' function
                    {{"u64"}, {"void"}},  // The 'u64' argument overload of the 'print' function
                    {{"f32"}, {"void"}},  // The 'f32' argument overload of the 'print' function
                    {{"f64"}, {"void"}},  // The 'f64' argument overload of the 'print' function
                    {{"char"}, {"void"}}, // The 'char' argument overload of the 'print' function
                    {{"str"}, {"void"}},  // The 'str' argument overload of the 'print' function
                    {{"bool"}, {"void"}}, // The 'bool' argument overload of the 'print' function
                }},
        }},  // End of the 'print' module,
    {"read", // The 'read' module
        {
            {"read_str", // The 'read_str' function
                {
                    {{}, {"str"}}, // The single version of the 'read_str' function
                }},
            {"read_i32", // The 'read_i32' function
                {
                    {{}, {"i32"}}, // The single version of the 'read_i32' function
                }},
            {"read_i64", // The 'read_i64' function
                {
                    {{}, {"i64"}}, // The single version of the 'read_i64' function
                }},
            {"read_u32", // The 'read_u32' function
                {
                    {{}, {"u32"}}, // The single version of the 'read_u32' function
                }},
            {"read_u64", // The 'read_u64' function
                {
                    {{}, {"u64"}}, // The single version of the 'read_u64' function
                }},
            {"read_f32", // The 'read_f32' function
                {
                    {{}, {"f32"}}, // The single version of the 'read_f32' function
                }},
            {"read_f64", // The 'read_f64' function
                {
                    {{}, {"f64"}}, // The single version of the 'read_f64' function
                }},

        }}, // End of the 'read' module
};

/// @enum `CFunctions`
/// @brief An enum used to access all the C functions
enum CFunction {
    PRINTF,
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
    STRLEN,
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
    {"i32x2", {"(i32, i32)", "str"}},
    {"i32x3", {"(i32, i32, i32)", "str"}},
    {"i32x4", {"(i32, i32, i32, i32)", "str"}},
    {"(i64, i64)", {"i64x2"}},
    {"(i64, i64, i64)", {"i64x3"}},
    {"(i64, i64, i64, i64)", {"i64x4"}},
    {"i64x2", {"(i64, i64)", "str"}},
    {"i64x3", {"(i64, i64, i64)", "str"}},
    {"i64x4", {"(i64, i64, i64, i64)", "str"}},
    {"(f32, f32)", {"f32x2"}},
    {"(f32, f32, f32)", {"f32x3"}},
    {"(f32, f32, f32, f32)", {"f32x4"}},
    {"f32x2", {"(f32, f32)", "str"}},
    {"f32x3", {"(f32, f32, f32)", "str"}},
    {"f32x4", {"(f32, f32, f32, f32)", "str"}},
    {"(f64, f64)", {"f64x2"}},
    {"(f64, f64, f64)", {"f64x3"}},
    {"(f64, f64, f64, f64)", {"f64x4"}},
    {"f64x2", {"(f64, f64)", "str"}},
    {"f64x3", {"(f64, f64, f64)", "str"}},
    {"f64x4", {"(f64, f64, f64, f64)", "str"}},
};
