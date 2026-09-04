#pragma once

#include "lexer.hpp"

#include <cassert>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

/// @class `JsonObject`
/// @brief Virtual abstract base class to represent all json objects
class JsonObject {
  public:
    virtual ~JsonObject() = default;
};

/// @class `JsonGroup`
/// @brief Class to represent a group (`"...": {...}`)
class JsonGroup : public JsonObject {
  public:
    JsonGroup(const std::string &name, std::vector<std::unique_ptr<JsonObject>> &fields) :
        name(name),
        fields(std::move(fields)) {}

    /// @var `name`
    /// @brief The name of the group
    std::string name;

    /// @var `fiels`
    /// @brief The fields of the group
    std::vector<std::unique_ptr<JsonObject>> fields;
};

/// @class `JsonString`
/// @brief Class to represent json string values (`"...": "..."`)
class JsonString : public JsonObject {
  public:
    JsonString(const std::string &name, const std::string &value) :
        name(name),
        value(value) {}

    /// @var `name`
    /// @brief The name of the field
    std::string name;

    /// @var `value`
    /// @brief The value of the string field
    std::string value;
};

/// @class `JsonNumber`
/// @brief Class to represent json number values (`"...": 1234`)
class JsonNumber : public JsonObject {
  public:
    JsonNumber(const std::string &name, const int number) :
        name(name),
        number(number) {}

    /// @var `name`
    /// @brief The name of the field
    std::string name;

    /// @var `number`
    /// @brief The number value of the field
    int number;
};

/// @class `JsonParser`
/// @brief Parses a given token stream of JsonTokens
class JsonParser {
  public:
    JsonParser() = delete;

    /// @function `extract_from_to`
    /// @brief Extracts a sub-vector from the given tokens vector, beginning at `from` to mutually exlusive `to`, modifying the `tokens`
    /// vector
    ///
    /// @param `tokens` The tokens to extract the sub-tokens from
    /// @param `from` The index from which to start
    /// @param `to` The index at which to end
    /// @return `std::vector<JsonToken>` The tokens `[from, to)`, extracted from the `tokens`
    static std::vector<JsonToken> extract_from_to(std::vector<JsonToken> &tokens, const size_t from, const size_t to) {
        assert(to >= from);
        assert(to <= tokens.size());
        std::vector<JsonToken> extraction;
        if (to == from) {
            return extraction;
        }
        extraction.reserve(to - from);
        std::copy(tokens.begin() + from, tokens.begin() + to, std::back_inserter(extraction));
        tokens.erase(tokens.begin() + from, tokens.begin() + to);
        return extraction;
    }

    /// @function `parse`
    /// @brief Parses the given tokens vector and retuns the JsonObject
    ///
    /// @param `tokens` The tokens to parse
    /// @return `JsonObject` The result of the parsing
    static std::optional<std::unique_ptr<JsonObject>> parse(std::vector<JsonToken> &tokens) {
        std::vector<std::unique_ptr<JsonObject>> objects;
        for (size_t i = 0; i < tokens.size(); i++) {
            if (tokens[i].type == JsonTokenType::TOK_LEFT_BRACE) {
                i++; // Skip the {
                size_t end_idx = i;
                int brace_depth = 1;
                while (end_idx < tokens.size() && brace_depth > 0) {
                    if (tokens[end_idx].type == JsonTokenType::TOK_LEFT_BRACE) {
                        brace_depth++;
                    } else if (tokens[end_idx].type == JsonTokenType::TOK_RIGHT_BRACE) {
                        brace_depth--;
                    }
                    end_idx++;
                }
                if (i >= tokens.size()) {
                    break;
                }
                std::vector<JsonToken> group_tokens = extract_from_to(tokens, i, end_idx - 1);
                std::optional<std::unique_ptr<JsonObject>> group_object = parse(group_tokens);
                if (!group_object.has_value()) {
                    std::cout << "Failed to parse group object" << std::endl;
                    return std::nullopt;
                }
                objects.emplace_back(std::move(group_object.value()));
            } else if (tokens[i].type == JsonTokenType::TOK_STR_VAL) {
                // The next token should be a colon
                const std::string identifier = tokens[i].content;
                i++;
                if (tokens[i].type != JsonTokenType::TOK_COLON) {
                    std::cout << "Error: expected ':' after name" << std::endl;
                    return std::nullopt;
                }
                i++;
                // Now it could either be: the beginning of an object, a number or a string value
                if (tokens[i].type == JsonTokenType::TOK_NUMBER) {
                    objects.emplace_back(std::make_unique<JsonNumber>(identifier, std::stoi(tokens[i].content)));
                } else if (tokens[i].type == JsonTokenType::TOK_STR_VAL) {
                    objects.emplace_back(std::make_unique<JsonString>(identifier, tokens[i].content));
                } else if (tokens[i].type == JsonTokenType::TOK_LEFT_BRACE) {
                    i++; // Skip the {
                    size_t end_idx = i;
                    int brace_depth = 1;
                    while (end_idx < tokens.size() && brace_depth > 0) {
                        if (tokens[end_idx].type == JsonTokenType::TOK_LEFT_BRACE) {
                            brace_depth++;
                        } else if (tokens[end_idx].type == JsonTokenType::TOK_RIGHT_BRACE) {
                            brace_depth--;
                        }
                        end_idx++;
                    }
                    if (i >= tokens.size()) {
                        break;
                    }
                    std::vector<JsonToken> group_tokens = extract_from_to(tokens, i, end_idx - 1);
                    std::optional<std::unique_ptr<JsonObject>> group_object = parse(group_tokens);
                    if (!group_object.has_value()) {
                        std::cout << "Failed to parse group object" << std::endl;
                        return std::nullopt;
                    }
                    if (auto json_group = dynamic_cast<JsonGroup *>(group_object.value().get())) {
                        if (json_group->name != "__ROOT__") {
                            std::cout << "Detected double naming in json group" << std::endl;
                            return std::nullopt;
                        }
                        objects.emplace_back(std::make_unique<JsonGroup>(identifier, json_group->fields));
                    } else {
                        objects.emplace_back(std::move(group_object.value()));
                    }
                } else {
                    assert(false);
                }
            }
        }
        if (objects.size() == 1) {
            if (auto group = dynamic_cast<const JsonGroup *>(objects.at(0).get())) {
                if (group->name == "__ROOT__") {
                    return std::move(objects.at(0));
                }
            }
        }
        return std::make_unique<JsonGroup>("__ROOT__", objects);
    }

    /// @function `to_string`
    /// @brief Converts a given json object to a string
    ///
    /// @param `object` The object to convert
    /// @param `indent_lvl` The indentation level of the recursive string conversion
    /// @note Calls itself recursively
    static std::string to_string(const JsonObject *object, int indent_lvl = 0) {
        std::stringstream ss;
        if (const auto group = dynamic_cast<const JsonGroup *>(object)) {
            if (group->name != "__ROOT__") {
                ss << std::string(indent_lvl, '\t') << "\"" << group->name << "\": ";
            } else {
                ss << std::string(indent_lvl, '\t');
            }
            ss << "{\n";
            for (auto it = group->fields.begin(); it != group->fields.end(); ++it) {
                if (it != group->fields.begin()) {
                    ss << ",\n";
                }
                ss << to_string((*it).get(), indent_lvl + 1);
                if (it == std::prev(group->fields.end())) {
                    ss << "\n";
                }
            }
            ss << std::string(indent_lvl, '\t') << "}";
        } else if (const auto string = dynamic_cast<const JsonString *>(object)) {
            ss << std::string(indent_lvl, '\t') << "\"" << string->name << "\": " << "\"" << string->value << "\"";
        } else if (const auto number = dynamic_cast<const JsonNumber *>(object)) {
            ss << std::string(indent_lvl, '\t') << "\"" << number->name << "\": " << number->number;
        }
        return ss.str();
    }

    /// @function `print_json_object`
    /// @brief Prints a given json object to the console
    ///
    /// @param `object` The object to print
    static void print_json_object(const JsonObject *object) {
        std::cout << to_string(object) << std::endl;
    }
};