#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

/// @class `JsonTokenType`
/// @brief Represents all possible tokens of a json file
enum class JsonTokenType {
    TOK_LEFT_BRACE,
    TOK_RIGHT_BRACE,
    TOK_COLON,
    TOK_COMMA,
    TOK_STR_VAL,
    TOK_NUMBER,
};

/// @struct `JsonToken`
/// @brief A simple json token
struct JsonToken {
  public:
    JsonToken(const JsonTokenType type, const std::string &content) :
        type(type),
        content(content) {}

    /// @var `type`
    /// @brief The type of the token
    JsonTokenType type;

    /// @var `content`
    /// @brief The content of the token
    std::string content;
};

class JsonLexer {
  public:
    JsonLexer() = delete;

    /// @function `scan`
    /// @brief Scans the given file and returns a list of all json tokens
    ///
    /// @param `file_path` The path the json file to scan is located at
    /// @return `std::vector<JsonToken>` A list of all scanned tokens
    static std::vector<JsonToken> scan(const std::filesystem::path &file_path) {
        // Load the given file
        std::ifstream file(file_path.string());
        if (!file) {
            throw std::runtime_error("Failed to load file " + file_path.string());
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string json_string = buffer.str();

        // Lex the given file
        std::vector<JsonToken> tokens;
        size_t start = 0;
        for (size_t end = 0; end < json_string.length(); end++) {
            switch (json_string[end]) {
                default:
                    if (is_digit(json_string[end])) {
                        end++;
                        while (json_string.begin() + end != json_string.end() && is_digit(json_string[end])) {
                            end++;
                        }
                        if (json_string.begin() + end == json_string.end()) {
                            std::cout << "Error: Json file ended with a number, not with a '}'" << std::endl;
                            return {};
                        }
                        tokens.emplace_back(JsonTokenType::TOK_NUMBER, json_string.substr(start, end - start));
                        start = end;
                        break;
                    }
                    std::cout << "Error: Unknown character in json string: '" << json_string[end] << "'" << std::endl;
                    return {};
                case '\n':
                    [[fallthrough]];
                case '\t':
                    [[fallthrough]];
                case '\r':
                    [[fallthrough]];
                case ' ':
                    start = end;
                    break;
                case '{':
                    tokens.emplace_back(JsonTokenType::TOK_LEFT_BRACE, "{");
                    start = end;
                    break;
                case '}':
                    tokens.emplace_back(JsonTokenType::TOK_RIGHT_BRACE, "}");
                    start = end;
                    break;
                case ':':
                    tokens.emplace_back(JsonTokenType::TOK_COLON, ":");
                    start = end;
                    break;
                case ',':
                    tokens.emplace_back(JsonTokenType::TOK_COMMA, ",");
                    start = end;
                    break;
                case '"':
                    end++;
                    start = end;
                    while (json_string.begin() + end != json_string.end() && json_string[end] != '"') {
                        end++;
                    }
                    if (json_string.begin() + end == json_string.end()) {
                        std::cout << "Error: Unterminated string value at the end of the json string" << std::endl;
                        return {};
                    }
                    tokens.emplace_back(JsonTokenType::TOK_STR_VAL, json_string.substr(start, end - start));
                    start = end;
                    break;
            }
        }
        return tokens;
    }

    /// @function `is_digit`
    /// @brief Returns whether a given character is a digit
    ///
    /// @param `c` The character to check
    /// @return `bool` Whether the character is a digit
    static bool is_digit(char c) {
        return c >= '0' && c <= '9';
    }

    /// @function `print_tokens`
    /// @brief Prints a given list of JsonTokens to the console
    ///
    /// @param `tokens` The list of tokens to print
    static void print_tokens(const std::vector<JsonToken> &tokens) {
        for (const auto &tok : tokens) {
            switch (tok.type) {
                case JsonTokenType::TOK_LEFT_BRACE:
                    std::cout << "TOK_LEFT_BRACE: ";
                    break;
                case JsonTokenType::TOK_RIGHT_BRACE:
                    std::cout << "TOK_RIGHT_BRACE: ";
                    break;
                case JsonTokenType::TOK_COLON:
                    std::cout << "TOK_COLON: ";
                    break;
                case JsonTokenType::TOK_COMMA:
                    std::cout << "TOK_COMMA ";
                    break;
                case JsonTokenType::TOK_STR_VAL:
                    std::cout << "TOK_STR_VAL: ";
                    break;
                case JsonTokenType::TOK_NUMBER:
                    std::cout << "TOK_NUMBER: ";
                    break;
            }
            std::cout << tok.content << std::endl;
        }
    }
};