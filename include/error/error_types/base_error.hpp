#pragma once

#include "error/error_type.hpp"
#include "lexer/token.hpp"
#include "parser/type/type.hpp"
#include "types.hpp"

#include <sstream>
#include <string>
#include <variant>

/// @class `BaseError`
/// @brief The base class which represents all possible error types in flint
class BaseError {
  public:
    [[nodiscard]] virtual std::string to_string() const;

    // destructor
    virtual ~BaseError() = default;
    // copy operators
    BaseError(const BaseError &) = default;
    BaseError &operator=(const BaseError &) = default;
    // move operators
    BaseError(BaseError &&) = default;
    BaseError &operator=(BaseError &&) = default;

  protected:
    BaseError(const ErrorType error_type, std::string file, int line, int column) :
        error_type(error_type),
        file(std::move(file)),
        line(line),
        column(column) {}

    /// @var `error_type`
    /// @brief The error type of the error, e.g. where it came from
    ErrorType error_type;

    /// @var `file`
    /// @brief The file the error occured
    std::string file;

    /// @var `line`
    /// @brief The line the error occured
    int line;

    /// @var `column`
    /// @biref The column the error occured
    int column;

    /// @function `trim_right`
    /// @brief Trims a given string from right (removes all trailing spaces from it)
    ///
    /// @param `str` The string to trim
    /// @return `std::string` The trimmed string
    [[nodiscard]] std::string trim_right(const std::string &str) const;

    /// @function `get_token_string`
    /// @brief Returns the string representation of a given token stream
    ///
    /// @param `tokens` The token stream to get the string from
    /// @return `std::string` The string representation of the given token stream
    [[nodiscard]] std::string get_token_string(const std::vector<Token> &tokens) const;

    /// @function `get_type_string`
    /// @brief Returns the string representation of a given type (group)
    ///
    /// @param `type` The type to get the string from
    /// @return `str::string` The string representation of the type
    [[nodiscard]] std::string get_type_string(const std::variant<std::shared_ptr<Type>, std::vector<std::shared_ptr<Type>>> &type) const;

    /// @function `get_token_string`
    /// @brief Returns the string representation of a given list of tokens, and tries to format it correctly too
    ///
    /// @param `tokens` The list of tokens whose string representation will be returned
    /// @param `ignore_tokens` The list of token types to ignore
    /// @return `std::string` The formatted string representation of the tokens
    [[nodiscard]] std::string get_token_string(const token_slice &tokens, const std::vector<Token> &ignore_tokens) const;

    /// @function `get_function_signature_string`
    /// @brief Returns the string that represents the signature of a given function. Used to check if function signatures match
    ///
    /// @param `function_name` The name of the function
    /// @param `arg_types` The types of the function arguments
    /// @return `std::string` The string representation of the functions signature (excluding its return type)
    [[nodiscard]] std::string get_function_signature_string( //
        const std::string &function_name,                    //
        const std::vector<std::shared_ptr<Type>> &arg_types  //
    ) const;

    /// @function `space_needed`
    /// @brief Returns whether the given iterator within the tokens list needs a space after it
    ///
    /// @param `tokens` The list of tokens in which to check
    /// @param `iterator` The iterator within the tokens list to check for
    /// @param `ignores` The token types for which the iterator does not need a space after it
    /// @return `bool` whether the given iterator in the token list needs a space after it
    [[nodiscard]] bool space_needed(               //
        const token_slice &tokens,                 //
        const token_list::const_iterator iterator, //
        const std::vector<Token> &ignores          //
    ) const;
};
