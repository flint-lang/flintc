#pragma once

#include <string_view>
#include <unordered_map>

enum ErrorType {
    ERR_NOT_IMPLEMENTED_YET = 0,
    ERR_LEXING = 1,
    ERR_PARSING = 2,
    ERR_RESOLVING = 3,
    ERR_SCOPE = 4,
    ERR_GENERATING = 5,
    ERR_LINKING = 6,
    ERR_FIP = 7,
};

static const std::unordered_map<ErrorType, std::string_view> error_type_names = {
    {ERR_LEXING, "Lexing Error"},
    {ERR_PARSING, "Parse Error"},
    {ERR_RESOLVING, "Resolve Error"},
    {ERR_SCOPE, "Scope Error"},
    {ERR_GENERATING, "Generate Error"},
    {ERR_LINKING, "Link Error"},
    {ERR_FIP, "FIP Error"},
};
