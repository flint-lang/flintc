#ifndef __ERROR_TYPE_HPP__
#define __ERROR_TYPE_HPP__

#include <string_view>
#include <unordered_map>

enum ErrorType {
    ERR_UNTERMINATED_STRING = 0,
    ERR_UNEXPECTED_TOKEN = 1,
    ERR_UNTERMINATED_MULTILINE_STRING = 2,
    ERR_NON_CHAR_VALUE_INSIDE_CHAR = 3,
    ERR_CHAR_LONGER_THAN_SINGLE_CHARACTER = 4,
    ERR_EXPECTED_PATH_AFTER_USE = 5,
    ERR_UNEXPECTED_DEFINITION = 6,
    ERR_NO_BODY_DECLARED = 7,
    ERR_USE_STATEMENT_MUST_BE_AT_TOP_LEVEL = 8,
    ERR_CONSTRUCTOR_NAME_DOES_NOT_MATCH_DATA_NAME = 9,
    ERR_CAN_ONLY_EXTEND_FROM_SINGLE_ERROR_SET = 10,
    ERR_ENTITY_CONSTRUCTOR_NAME_DOES_NOT_MATCH_ENTITY_NAME = 11,
    ERR_UNDEFINED_STATEMENT = 12,
    ERR_PARSING = 13,
    ERR_NOT_IMPLEMENTED_YET = 14,
    ERR_DEBUG = 15,
    ERR_UNDEFINED_EXPRESSION = 16,
    ERR_LINKING = 17,
    ERR_GENERATING = 18,
    ERR_SCOPE = 19,
    ERR_RESOLVING = 20,
    ERR_LEXING = 21,
};

static const std::unordered_map<ErrorType, std::string_view> error_type_names = {
    {ERR_PARSING, "Parse Error"},
    {ERR_GENERATING, "Generate Error"},
};

#endif
