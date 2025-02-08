#ifndef __ERROR_HPP__
#define __ERROR_HPP__

#include "error_type.hpp"

// All error types are included here to make using the error header file easier
#include "error_types/base_error.hpp"

#include "error_types/lexing/comments/err_comment_unterm_multiline.hpp"
#include "error_types/lexing/err_lexing.hpp"
#include "error_types/lexing/literals/err_lit_char_longer_than_single_character.hpp"
#include "error_types/lexing/literals/err_lit_expected_char_value.hpp"
#include "error_types/lexing/literals/err_lit_unterminated_string.hpp"
#include "error_types/lexing/unexpected/err_unexpected_token.hpp"
#include "error_types/lexing/unexpected/err_unexpected_token_number.hpp"
#include "error_types/lexing/unexpected/err_unexpected_token_pipe.hpp"

#include "error_types/parsing/definitions/err_unexpected_definition.hpp"
#include "error_types/parsing/definitions/err_use_statement_not_at_top_level.hpp"
#include "error_types/parsing/err_parsing.hpp"
#include "error_types/parsing/expressions/err_expr_binop_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_binop_type_mismatch.hpp"
#include "error_types/parsing/expressions/err_expr_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_type_mismatch.hpp"
#include "error_types/parsing/scopes/err_missing_body.hpp"
#include "error_types/parsing/values/err_val_unknown_literal.hpp"
#include "error_types/parsing/variables/err_var_from_requires_list.hpp"
#include "error_types/parsing/variables/err_var_not_declared.hpp"
#include "error_types/parsing/variables/err_var_redefinition.hpp"

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
