#pragma once

#include <string_view>
#include <unordered_map>

enum ErrorType {
    ERR_NOT_IMPLEMENTED_YET = 0,
    ERR_DEBUG = 1,
    ERR_LEXING = 2,
    ERR_PARSING = 3,
    ERR_RESOLVING = 4,
    ERR_SCOPE = 5,
    ERR_GENERATING = 6,
    ERR_LINKING = 7,
};

static const std::unordered_map<ErrorType, std::string_view> error_type_names = {
    {ERR_LEXING, "Lexing Error"},
    {ERR_PARSING, "Parse Error"},
    {ERR_RESOLVING, "Resolve Error"},
    {ERR_SCOPE, "Scope Error"},
    {ERR_GENERATING, "Generate Error"},
    {ERR_LINKING, "Link Error"},
};
