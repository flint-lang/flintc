#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

/// @typedef `type_list`
/// @brief This type is used for a list of types in the `builtin_function_types` map to make its signature much clearer
using type_list = std::vector<std::string_view>;

/// @typedef `overloads`
/// @brief This type is used for all overloads of a function, it contains the argument and return types of a function and whether the
/// function can return an error, for Flints internal error handling system
using overloads = std::vector<std::tuple<type_list, type_list, bool>>;

/// @typedef `function_list`
/// @brief A function list is a list of functions together with its signature overloads
using function_overload_list = std::unordered_map<std::string_view, overloads>;

/// @var `core_module_functions`
/// @brief A map containing all core modules and maps the module to its functions
static inline std::unordered_map<std::string_view, function_overload_list> core_module_functions = {
    {"assert", // The 'assert' module
        {
            {"assert", // The 'assert' function
                {
                    {{"bool"}, {"void"}, true}, // The single version of the 'assert' function
                }},
        }},   // End of the 'assert' module
    {"print", // The 'print' module
        {
            {"print", // The 'print' function
                {
                    {{"i32"}, {"void"}, false},                  // The 'i32' argument overload of the 'print' function
                    {{"i64"}, {"void"}, false},                  // The 'i64' argument overload of the 'print' function
                    {{"u32"}, {"void"}, false},                  // The 'u32' argument overload of the 'print' function
                    {{"u64"}, {"void"}, false},                  // The 'u64' argument overload of the 'print' function
                    {{"f32"}, {"void"}, false},                  // The 'f32' argument overload of the 'print' function
                    {{"f64"}, {"void"}, false},                  // The 'f64' argument overload of the 'print' function
                    {{"u8"}, {"void"}, false},                   // The 'u8' argument overload of the 'print' function
                    {{"str"}, {"void"}, false},                  // The 'str' argument overload of the 'print' function
                    {{"__flint_type_str_lit"}, {"void"}, false}, // The 'str' literal argument overload of the 'print' function
                    {{"bool"}, {"void"}, false},                 // The 'bool' argument overload of the 'print' function
                }},
        }},  // End of the 'print' module
    {"read", // The 'read' module
        {
            {"read_str", // The 'read_str' function
                {
                    {{}, {"str"}, false}, // The single version of the 'read_str' function
                }},
            {"read_i32", // The 'read_i32' function
                {
                    {{}, {"i32"}, true}, // The single version of the 'read_i32' function
                }},
            {"read_i64", // The 'read_i64' function
                {
                    {{}, {"i64"}, true}, // The single version of the 'read_i64' function
                }},
            {"read_u32", // The 'read_u32' function
                {
                    {{}, {"u32"}, true}, // The single version of the 'read_u32' function
                }},
            {"read_u64", // The 'read_u64' function
                {
                    {{}, {"u64"}, true}, // The single version of the 'read_u64' function
                }},
            {"read_f32", // The 'read_f32' function
                {
                    {{}, {"f32"}, true}, // The single version of the 'read_f32' function
                }},
            {"read_f64", // The 'read_f64' function
                {
                    {{}, {"f64"}, true}, // The single version of the 'read_f64' function
                }},
        }},        // End of the 'read' module
    {"filesystem", // The 'filesystem' module
        {
            {"read_file", // The 'read_file' function
                {
                    {{"str"}, {"str"}, true}, // The single version of the 'read_file' function
                }},
            {"read_lines", // The 'read_lines' function
                {
                    {{"str"}, {"str[]"}, true}, // The single version of the 'read_lines' function
                }},
            {"file_exists", // The 'file_exists' function
                {
                    {{"str"}, {"bool"}, false}, // The single version of the 'file_exists' function
                }},
            {"write_file", // The 'write_file' function
                {
                    {{"str", "str"}, {"void"}, true}, // The single version of the 'write_file' function
                }},
            {"append_file", // The 'append_file' function
                {
                    {{"str", "str"}, {"void"}, true}, // The single version of the 'append_file' function
                }},
            {"is_file", // The 'is_file' function
                {
                    {{"str"}, {"bool"}, false}, // The single version of the 'is_file' function
                }},
        }}, // End of the 'filesystem' module
    {"env", // The 'env' module
        {
            {"get_env", // The 'get_env' function
                {
                    {{"str"}, {"str"}, true}, // The single version of the 'get_env' function
                }},
            {"set_env", // The 'set_env' function
                {
                    {{"str", "str", "bool"}, {"bool"}, false}, // The single version of the 'set_env' function
                }},
        }},    // End of the 'env' module
    {"system", // The 'system' module
        {
            {"system_command", // The 'system_command' function
                {
                    {{"str"}, {"i32", "str"}, true}, // The single version of the 'system' function
                }},
        }} // End of the 'system' module
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
    FOPEN,
    FSEEK,
    FCLOSE,
    FTELL,
    FREAD,
    REWIND,
    FGETS,
    FWRITE,
    GETENV,
    SETENV,
    POPEN,
    PCLOSE,
};

static inline std::unordered_map<std::string_view, std::vector<std::string_view>> primitive_casting_table = {
    {"__flint_type_str_lit", {"str"}},
    {"i32", {"str", "u8", "i64", "f32", "f64", "u32", "u64"}},
    {"i64", {"str", "u8", "i32", "f32", "f64", "u32", "u64"}},
    {"u32", {"str", "u8", "i32", "i64", "f32", "f64", "u64"}},
    {"u64", {"str", "u8", "i32", "i64", "f32", "f64", "u32"}},
    {"f32", {"str", "i32", "i64", "f64", "u32", "u64"}},
    {"f64", {"str", "i32", "i64", "f32", "u32", "u64"}},
    {"u8", {"bool8", "str", "i32", "i64", "u32", "u64"}},
    {"bool", {"str"}},
    {"bool8", {"str", "u8"}},
};

static inline std::unordered_map<std::string_view, std::vector<std::string_view>> primitive_implicit_casting_table = {
    {"__flint_type_str_lit", {"str"}},
    {"i32", {"str", "u32", "u64", "i64", "f32", "f64", "i32x2", "i32x3", "i32x4", "i32x8"}},
    {"i64", {"str", "i64x2", "i64x3", "i64x4"}},
    {"u32", {"str", "i32", "i64", "u64", "f32", "f64"}},
    {"u64", {"str"}},
    {"f32", {"str", "f64", "f32x2", "f32x3", "f32x4", "f32x8"}},
    {"f64", {"str", "f64x2", "f64x3", "f64x4"}},
    {"bool", {"str"}},
    {"u8", {"bool8", "str", "i32", "u32", "i64", "u64"}},
    {"bool8", {"u8", "str"}},
    {"(i32, i32)", {"i32x2"}},
    {"(i32, i32, i32)", {"i32x3"}},
    {"(i32, i32, i32, i32)", {"i32x4"}},
    {"(i32, i32, i32, i32, i32, i32, i32, i32)", {"i32x8"}},
    {"i32x2", {"(i32, i32)", "str"}},
    {"i32x3", {"(i32, i32, i32)", "str"}},
    {"i32x4", {"(i32, i32, i32, i32)", "str"}},
    {"i32x8", {"(i32, i32, i32, i32, i32, i32, i32, i32)", "str"}},
    {"(i64, i64)", {"i64x2"}},
    {"(i64, i64, i64)", {"i64x3"}},
    {"(i64, i64, i64, i64)", {"i64x4"}},
    {"i64x2", {"(i64, i64)", "str"}},
    {"i64x3", {"(i64, i64, i64)", "str"}},
    {"i64x4", {"(i64, i64, i64, i64)", "str"}},
    {"(f32, f32)", {"f32x2"}},
    {"(f32, f32, f32)", {"f32x3"}},
    {"(f32, f32, f32, f32)", {"f32x4"}},
    {"(f32, f32, f32, f32, f32, f32, f32, f32)", {"f32x8"}},
    {"f32x2", {"(f32, f32)", "str"}},
    {"f32x3", {"(f32, f32, f32)", "str"}},
    {"f32x4", {"(f32, f32, f32, f32)", "str"}},
    {"f32x8", {"(f32, f32, f32, f32, f32, f32, f32, f32)", "str"}},
    {"(f64, f64)", {"f64x2"}},
    {"(f64, f64, f64)", {"f64x3"}},
    {"(f64, f64, f64, f64)", {"f64x4"}},
    {"f64x2", {"(f64, f64)", "str"}},
    {"f64x3", {"(f64, f64, f64)", "str"}},
    {"f64x4", {"(f64, f64, f64, f64)", "str"}},
};
