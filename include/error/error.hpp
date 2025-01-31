#ifndef __ERROR_HPP__
#define __ERROR_HPP__

#include "error_type.hpp"

// All error types are included here to make using the error header file easier
#include "error/error_types/base_error.hpp"
#include "error_types/lexing/err_lexing.hpp"
#include "error_types/parsing/err_parsing.hpp"

#include <iostream>
#include <string>
#include <type_traits>

/// @brief Throws the given ErrorType as an runtime error to the console. Very basic error handling
///
/// @param error_type The error enum type, whose Enum ID will be printed to the console
inline void throw_err(ErrorType error_type) {
    throw std::runtime_error("Custom Error: " + std::to_string(static_cast<int>(error_type)));
}

/// @brief Throws a custom error and exits the program
/// @details Creates an error object of the specified type and prints its message to stderr before
///          terminating the program. The error type must be derived from BaseError but cannot be
///          BaseError itself.
///
/// @tparam ErrorType The type of error to throw. Must inherit from BaseError.
/// @tparam Args Variable template parameter pack for constructor arguments
///
/// @param args Arguments to forward to the ErrorType constructor
///
/// @throws Nothing directly (calls std::exit)
///
/// @note This function never returns as it calls std::exit
///
/// @example `throw_err<ErrParsing>("Syntax error", "file.txt", 10, 5, tokens);`
template <typename ErrorType, typename... Args>                                                    //
std::enable_if_t<std::is_base_of_v<BaseError, ErrorType> && !std::is_same_v<BaseError, ErrorType>> //
throw_err(Args &&...args) {
    ErrorType error(std::forward<Args>(args)...);
    std::cerr << error.to_string() << std::endl;
    std::exit(EXIT_FAILURE);
}

#endif
