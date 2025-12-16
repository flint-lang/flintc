#pragma once

#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>

/// @typedef `string_list`
/// @brief A simple alias for a vector of string views
using string_list = std::vector<std::string_view>;

/// @typedef `string_pair_list`
/// @brief A simple list of string pairs
using string_pair_list = std::vector<std::pair<std::string_view, std::string_view>>;

/// @typedef `overloads`
/// @brief This type is used for all overloads of a function, it contains the argument and return types of a function and a list of all the
/// error sets the function could throw
using overloads = std::vector<std::tuple<string_pair_list, string_list, string_list>>;

/// @typedef `function_overload_list`
/// @brief A function list is a list of functions together with its signature overloads
using function_overload_list = std::unordered_map<std::string_view, overloads>;

/// @var `core_module_functions`
/// @brief A map containing all core modules and maps the module to its functions
static const inline std::map<std::string_view, function_overload_list> core_module_functions = {
    {"print", // The 'print' module
        {
            {"print", // The 'print' function
                {
                    {{{"i32", "value"}}, {"void"}, {}},                  // The 'i32' argument overload of the 'print' function
                    {{{"i64", "value"}}, {"void"}, {}},                  // The 'i64' argument overload of the 'print' function
                    {{{"u32", "value"}}, {"void"}, {}},                  // The 'u32' argument overload of the 'print' function
                    {{{"u64", "value"}}, {"void"}, {}},                  // The 'u64' argument overload of the 'print' function
                    {{{"f32", "value"}}, {"void"}, {}},                  // The 'f32' argument overload of the 'print' function
                    {{{"f64", "value"}}, {"void"}, {}},                  // The 'f64' argument overload of the 'print' function
                    {{{"u8", "value"}}, {"void"}, {}},                   // The 'u8' argument overload of the 'print' function
                    {{{"str", "value"}}, {"void"}, {}},                  // The 'str' argument overload of the 'print' function
                    {{{"__flint_type_str_lit", "value"}}, {"void"}, {}}, // The 'str' literal argument overload of the 'print' function
                    {{{"bool", "value"}}, {"void"}, {}},                 // The 'bool' argument overload of the 'print' function
                }},
        }},  // End of the 'print' module
    {"read", // The 'read' module
        {
            {"read_str", // The 'read_str' function
                {
                    {{}, {"str"}, {}}, // The single version of the 'read_str' function
                }},
            {"read_i32", // The 'read_i32' function
                {
                    {{}, {"i32"}, {"ErrRead"}}, // The single version of the 'read_i32' function
                }},
            {"read_i64", // The 'read_i64' function
                {
                    {{}, {"i64"}, {"ErrRead"}}, // The single version of the 'read_i64' function
                }},
            {"read_u32", // The 'read_u32' function
                {
                    {{}, {"u32"}, {"ErrRead"}}, // The single version of the 'read_u32' function
                }},
            {"read_u64", // The 'read_u64' function
                {
                    {{}, {"u64"}, {"ErrRead"}}, // The single version of the 'read_u64' function
                }},
            {"read_f32", // The 'read_f32' function
                {
                    {{}, {"f32"}, {"ErrRead"}}, // The single version of the 'read_f32' function
                }},
            {"read_f64", // The 'read_f64' function
                {
                    {{}, {"f64"}, {"ErrRead"}}, // The single version of the 'read_f64' function
                }},
        }},    // End of the 'read' module
    {"assert", // The 'assert' module
        {
            {"assert", // The 'assert' function
                {
                    {{{"bool", "condition"}}, {"void"}, {"ErrAssert"}}, // The single version of the 'assert' function
                }},
        }},        // End of the 'assert' module
    {"filesystem", // The 'filesystem' module
        {
            {"read_file", // The 'read_file' function
                {
                    {{{"str", "path"}}, {"str"}, {"ErrIO"}}, // The single version of the 'read_file' function
                }},
            {"read_lines", // The 'read_lines' function
                {
                    {{{"str", "path"}}, {"str[]"}, {"ErrFS"}}, // The single version of the 'read_lines' function
                }},
            {"file_exists", // The 'file_exists' function
                {
                    {{{"str", "path"}}, {"bool"}, {}}, // The single version of the 'file_exists' function
                }},
            {"write_file", // The 'write_file' function
                {
                    {{{"str", "path"}, {"str", "value"}}, {"void"}, {"ErrFS"}}, // The single version of the 'write_file' function
                }},
            {"append_file", // The 'append_file' function
                {
                    {{{"str", "path"}, {"str", "content"}}, {"void"}, {"ErrFS"}}, // The single version of the 'append_file' function
                }},
            {"is_file", // The 'is_file' function
                {
                    {{{"str", "path"}}, {"bool"}, {}}, // The single version of the 'is_file' function
                }},
        }}, // End of the 'filesystem' module
    {"env", // The 'env' module
        {
            {"get_env", // The 'get_env' function
                {
                    {{{"str", "env_name"}}, {"str"}, {"ErrEnv"}}, // The single version of the 'get_env' function
                }},
            {"set_env", // The 'set_env' function
                {
                    {{{"str", "env_name"}, {"str", "value"}, {"bool", "override"}}, {"bool"},
                        {"ErrEnv"}}, // The single version of the 'set_env' function
                }},
        }},    // End of the 'env' module
    {"system", // The 'system' module
        {
            {"system_command", // The 'system_command' function
                {
                    {{{"str", "command"}}, {"i32", "str"}, {"ErrSystem"}}, // The single version of the 'system' function
                }},
        }},  // End of the 'system' module
    {"math", // The 'math' module
        {
            {"sin", // The 'sin' function
                {
                    {{{"f32", "value"}}, {"f32"}, {}}, // The 'f32' overload of the 'sin' function
                    {{{"f64", "value"}}, {"f64"}, {}}, // The 'f64' overload of the 'sin' function
                }},
            {"cos", // The 'cos' function
                {
                    {{{"f32", "value"}}, {"f32"}, {}}, // The 'f32' overload of the 'cos' function
                    {{{"f64", "value"}}, {"f64"}, {}}, // The 'f64' overload of the 'cos' function
                }},
            {"sqrt", // The 'sqrt' function
                {
                    {{{"f32", "value"}}, {"f32"}, {}}, // The 'f32' overload of the 'cos' function
                    {{{"f64", "value"}}, {"f64"}, {}}, // The 'f64' overload of the 'cos' function
                }},
            {"abs", // The 'abs' function
                {
                    {{{"i32", "value"}}, {"i32"}, {}}, // The 'i32' overload of the 'abs' function
                    {{{"i64", "value"}}, {"i64"}, {}}, // The 'i64' overload of the 'abs' function
                    {{{"f32", "value"}}, {"f32"}, {}}, // The 'f32' overload of the 'abs' function
                    {{{"f64", "value"}}, {"i64"}, {}}, // The 'f64' overload of the 'abs' function
                }},
            {"min", // The 'min' function
                {
                    {{{"u32", "value1"}, {"u32", "value2"}}, {"u32"}, {}}, // The 'u32' overload of the 'min' function
                    {{{"i32", "value1"}, {"i32", "value2"}}, {"i32"}, {}}, // The 'i32' overload of the 'min' function
                    {{{"f32", "value1"}, {"f32", "value2"}}, {"f32"}, {}}, // The 'f32' overload of the 'min' function
                    {{{"u64", "value1"}, {"u64", "value2"}}, {"u64"}, {}}, // The 'u64' overload of the 'min' function
                    {{{"i64", "value1"}, {"i64", "value2"}}, {"i64"}, {}}, // The 'i64' overload of the 'min' function
                    {{{"f64", "value1"}, {"f64", "value2"}}, {"f64"}, {}}, // The 'f64' overload of the 'min' function
                }},
            {"max", // The 'max' function
                {
                    {{{"u32", "value1"}, {"u32", "value2"}}, {"u32"}, {}}, // The 'u32' overload of the 'max' function
                    {{{"i32", "value1"}, {"i32", "value2"}}, {"i32"}, {}}, // The 'i32' overload of the 'max' function
                    {{{"f32", "value1"}, {"f32", "value2"}}, {"f32"}, {}}, // The 'f32' overload of the 'max' function
                    {{{"u64", "value1"}, {"u64", "value2"}}, {"u64"}, {}}, // The 'u64' overload of the 'max' function
                    {{{"i64", "value1"}, {"i64", "value2"}}, {"i64"}, {}}, // The 'i64' overload of the 'max' function
                    {{{"f64", "value1"}, {"f64", "value2"}}, {"f64"}, {}}, // The 'f64' overload of the 'max' function
                }},
        }},   // End of the 'math' module
    {"parse", // The 'parse' module
        {
            {"parse_u8", // The 'parse_u8' function
                {
                    {{{"str", "input"}}, {"u8"}, {"ErrParse"}}, // The single version of the 'parse_u8' function
                }},
            {"parse_i32", // The 'parse_i32' function
                {
                    {{{"str", "input"}}, {"i32"}, {"ErrParse"}}, // The single version of the 'parse_i32' function
                }},
            {"parse_i64", // The 'parse_i64' function
                {
                    {{{"str", "input"}}, {"i64"}, {"ErrParse"}}, // The single version of the 'parse_i64' function
                }},
            {"parse_u32", // The 'parse_u32' function
                {
                    {{{"str", "input"}}, {"u32"}, {"ErrParse"}}, // The single version of the 'parse_u32' function
                }},
            {"parse_u64", // The 'parse_u64' function
                {
                    {{{"str", "input"}}, {"u64"}, {"ErrParse"}}, // The single version of the 'parse_u64' function
                }},
            {"parse_f32", // The 'parse_f32' function
                {
                    {{{"str", "input"}}, {"f32"}, {"ErrParse"}}, // The single version of the 'parse_f32' function
                }},
            {"parse_f64", // The 'parse_f64' function
                {
                    {{{"str", "input"}}, {"f64"}, {"ErrParse"}}, // The single version of the 'parse_f64' function
                }},
        }},  // End of the 'parse' module
    {"time", // The 'time' module
        {
            {"now", // The 'now' function
                {
                    {{}, {"TimeStamp"}, {}}, // The single version of the 'now' function
                }},
            {"duration", // The 'duration' function
                {
                    {{{"TimeStamp", "from"}, {"TimeStamp", "to"}}, {"Duration"}, {}}, // The single version of the 'duration' function
                }},
            {"as_unit", // The 'as_unit' function
                {
                    {{{"Duration", "duration"}, {"TimeUnit", "unit"}}, {"f64"}, {}}, // The single version of the 'as' function
                }},
            {"sleep", // The 'sleep' function
                {
                    {{{"Duration", "duration"}}, {}, {}},               // The first version of the 'sleep' function
                    {{{"u64", "value"}, {"TimeUnit", "unit"}}, {}, {}}, // The second version of the 'sleep' function
                }},
            {"from", // The 'from' function
                {
                    {{{"u64", "value"}, {"TimeUnit", "unit"}}, {"Duration"}, {}}, // The single version of the 'from' function
                }},
        }} // End of the 'time' module
};

/// @typedef `error_value`
/// @brief The value of an error together with the description of that error value
using error_value = std::pair<std::string_view, std::string_view>;

/// @typedef `error_set`
/// @brief All values needed to construct an error node from:
///     - The name of the error set type
///     - The parent this error set type extends
///     - The list of all error values from this set
using error_set = std::tuple<std::string_view, std::string_view, std::vector<error_value>>;

/// @var `core_module_error_sets`
/// @brief A map containing all core modules and maps the module to all the error sets it provides
static const inline std::unordered_map<std::string_view, std::vector<error_set>> core_module_error_sets = {
    {
        "assert",                  // The 'assert' module
        {{"ErrAssert", "anyerror", // The 'ErrAssert' error set, Value 0 is `anyerror`
            {
                {"AssertionFailed", "The assertion has failed"} // Value 1
            }}},
    }, // End of the 'assert' module
    {
        "read",                  // The 'read' module
        {{"ErrRead", "anyerror", // The 'ErrRead' error set
            {
                {"ReadLines", "Could not read lines from console"},                   // Value 0
                {"ParseInt", "Could not parse text to integer"},                      // Value 1
                {"NegativeUint", "Negative input not allowed for unsigned integers"}, // Value 2
                {"ParseFloat", "Could not parse text to floating‑point"}              // Value 3
            }}},
    }, // End of the 'read' module
    {
        "filesystem", // The 'filesystem' module
        {
            {"ErrIO", "anyerror", // The 'ErrIO' error set
                {
                    {"OpenFailed", "Could not open the file"},                   // Value 0
                    {"NotFound", "File does not exist"},                         // Value 1
                    {"NotReadable", "Exists but is not readable"},               // Value 2
                    {"NotWritable", "Exists but is not writable (permissions)"}, // Value 3
                    {"UnexpectedEOF", "Hit EOF in the middle of a read"}         // Value 4
                }},
            {"ErrFS", "ErrIO", // The 'ErrFS' error set, Extends all values from `ErrIO`
                {
                    {"TooLarge", "File is unreasonably large"}, // Value 5
                    {"InvalidPath", "Path string is malformed"} // Value 6
                }},
        },
    }, // End of the 'filesystem' module
    {
        "env",                  // The 'env' module
        {{"ErrEnv", "anyerror", // The 'ErrEnv' error set
            {
                {"VarNotFound", "Requested variable not set"},               // Value 0
                {"InvalidName", "Name contains illegal characters"},         // Value 1
                {"InvalidValue", "Value cannot be used (e.g. embedded NUL)"} // Value 2
            }}},
    }, // End of the 'env' module
    {
        "system",                  // The 'system' module
        {{"ErrSystem", "anyerror", // The 'ErrSystem' error set
            {
                {"SpawnFailed", "Process could not be created"}, // Value 0
            }}},
    }, // End of the 'system' module
    {
        "parse",                  // The 'parse' module
        {{"ErrParse", "anyerror", // The 'ErrParse' error set
            {
                {"OutOfBounds", "The input is out of the bounds of the result"},           // Value 0
                {"InvalidCharacter", "The input contains one or more invalid characters"}, // Value 1
            }}},
    }, // End of the 'parse' module
};

/// @typedef `enum_type`
/// @brief The type representation of an enum:
///     The name of the enum type
///     All values the enum could have
using enum_type = std::pair<std::string_view, std::vector<std::string_view>>;

/// @var `core_module_enum_types`
/// @brief A map containing all core modules and maps each module to all enum types it provides
static const inline std::unordered_map<std::string_view, std::vector<enum_type>> core_module_enum_types = {
    {
        "time", // The 'time' module
        {
            {"TimeUnit", {"NS", "US", "MS", "S"}}, // The 'TimeUnit' enum type
        },
    }, // End of the 'time' module
};

/// @typedef `data_field`
/// @brief The field representation of a data field, being `type` and `name` of the fields
using data_field = std::pair<std::string_view, std::string_view>;

/// @typedef `data_type`
/// @brief The type representation of data:
///     The name of the data type
///     A list of fields
///     A list of the field's positions in the constructor of the data type, e.g. in which order to construct the fields
using data_type = std::tuple<std::string_view, std::vector<data_field>, std::vector<size_t>>;

/// @var `core_module_data_types`
/// @brief A map containning all core modules and maps each module to all data types it provides
static const inline std::unordered_map<std::string_view, std::vector<data_type>> core_module_data_types = {
    {
        "time", // The 'time' module
        {
            {"TimeStamp", {{"u64", "value"}}, {0}}, // The 'TimeStamp' data type
            {"Duration", {{"u64", "value"}}, {0}},  // The 'Duration' data type
        },
    }, // End of the 'time' module
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
    SIN,
    SINF,
    COS,
    COSF,
    SQRT,
    SQRTF,
    POW,
    POWF,
    ABS,
    LABS,
    FABSF,
    FABS,
};

static const inline std::unordered_map<std::string_view, std::vector<std::string_view>> primitive_casting_table = {
    {"__flint_type_str_lit", {"str"}},
    {"int", {"str", "u8", "u32", "u64", "i32", "i64", "f32", "f64"}},
    {"i32", {"str", "u8", "i64", "f32", "f64", "u32", "u64"}},
    {"i64", {"str", "u8", "i32", "f32", "f64", "u32", "u64"}},
    {"u32", {"str", "u8", "i32", "i64", "f32", "f64", "u64"}},
    {"u64", {"str", "u8", "i32", "i64", "f32", "f64", "u32"}},
    {"float", {"str", "u8", "i32", "i64", "u32", "u64", "f32", "f64"}},
    {"f32", {"str", "i32", "i64", "f64", "u32", "u64"}},
    {"f64", {"str", "i32", "i64", "f32", "u32", "u64"}},
    {"u8", {"bool8", "str", "i32", "i64", "u32", "u64"}},
    {"bool", {"str", "u8", "u32", "u64", "i32", "i64", "f32", "f64"}},
    {"bool8", {"str", "u8"}},
};

static const inline std::unordered_map<std::string_view, std::vector<std::string_view>> primitive_implicit_casting_table = {
    {"__flint_type_str_lit", {"str"}},
    {"int", {"str", "u8", "u32", "u64", "i32", "i64", "f32", "f64"}},
    {"i32", {"str", "i64", "f64", "i32x2", "i32x3", "i32x4", "i32x8"}},
    {"i64", {"str", "i64x2", "i64x3", "i64x4"}},
    {"u32", {"str", "i64", "u64", "f32", "f64"}},
    {"u64", {"str"}},
    {"float", {"str", "f32", "f64"}},
    {"f32", {"str", "f64", "f32x2", "f32x3", "f32x4", "f32x8"}},
    {"f64", {"str", "f64x2", "f64x3", "f64x4"}},
    {"bool", {"str"}},
    {"u8", {"str", "bool8", "u32", "i32", "f32", "u64", "i64", "f64", "u8x2", "u8x3", "u8x4", "u8x8"}},
    {"bool8", {"u8", "str"}},
    {"(u8, u8)", {"u8x2"}},
    {"(u8, u8, u8)", {"u8x3"}},
    {"(u8, u8, u8, u8)", {"u8x4"}},
    {"(u8, u8, u8, u8, u8, u8, u8, u8)", {"u8x8"}},
    {"(i32, i32)", {"i32x2"}},
    {"(i32, i32, i32)", {"i32x3"}},
    {"(i32, i32, i32, i32)", {"i32x4"}},
    {"(i32, i32, i32, i32, i32, i32, i32, i32)", {"i32x8"}},
    {"i32x2", {"(i32, i32)", "str"}},
    {"i32x3", {"(i32, i32, i32)", "str"}},
    {"i32x4", {"(i32, i32, i32, i32)", "str"}},
    {"i32x8", {"(i32, i32, i32, i32, i32, i32, i32, i32)", "str"}},
    {"u8x2", {"(u8, u8)", "str"}},
    {"u8x3", {"(u8, u8, u8)", "str"}},
    {"u8x4", {"(u8, u8, u8, u8)", "str"}},
    {"u8x8", {"(u8, u8, u8, u8, u8, u8, u8, u8)", "str"}},
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
