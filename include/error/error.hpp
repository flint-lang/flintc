#ifndef __ERROR_HPP__
#define __ERROR_HPP__

#include "error_type.hpp"

// All error types are included here to make using the error header file easier
#include "error_types/base_error.hpp"

#include "error_types/lexing/err_lexing.hpp"

#include "error_types/lexing/comments/err_comment_unterm_multiline.hpp"

#include "error_types/lexing/literals/err_lit_char_longer_than_single_character.hpp"
#include "error_types/lexing/literals/err_lit_expected_char_value.hpp"
#include "error_types/lexing/literals/err_lit_unterminated_string.hpp"

#include "error_types/lexing/unexpected/err_unexpected_token.hpp"
#include "error_types/lexing/unexpected/err_unexpected_token_number.hpp"
#include "error_types/lexing/unexpected/err_unexpected_token_pipe.hpp"

#include "error_types/parsing/err_parsing.hpp"

#include "error_types/parsing/definitions/err_def_err_only_one_parent.hpp"
#include "error_types/parsing/definitions/err_def_func_creation.hpp"
#include "error_types/parsing/definitions/err_def_function_creation.hpp"
#include "error_types/parsing/definitions/err_unexpected_definition.hpp"
#include "error_types/parsing/definitions/err_use_statement_not_at_top_level.hpp"

#include "error_types/parsing/expressions/err_expr_binop_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_binop_type_mismatch.hpp"
#include "error_types/parsing/expressions/err_expr_call_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_call_of_undefined_function.hpp"
#include "error_types/parsing/expressions/err_expr_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_lit_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_type_mismatch.hpp"
#include "error_types/parsing/expressions/err_expr_unop_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_variable_creation_failed.hpp"

#include "error_types/parsing/scopes/err_body_creation_failed.hpp"
#include "error_types/parsing/scopes/err_missing_body.hpp"
#include "error_types/parsing/scopes/err_unclosed_paren.hpp"

#include "error_types/parsing/statements/err_stmt_assignment_creation_failed.hpp"
#include "error_types/parsing/statements/err_stmt_catch_creation_failed.hpp"
#include "error_types/parsing/statements/err_stmt_creation_failed.hpp"
#include "error_types/parsing/statements/err_stmt_dangling_catch.hpp"
#include "error_types/parsing/statements/err_stmt_dangling_else.hpp"
#include "error_types/parsing/statements/err_stmt_dangling_equal_sign.hpp"
#include "error_types/parsing/statements/err_stmt_declaration_creation_failed.hpp"
#include "error_types/parsing/statements/err_stmt_for_creation_failed.hpp"
#include "error_types/parsing/statements/err_stmt_if_chain_missing_if.hpp"
#include "error_types/parsing/statements/err_stmt_if_creation_failed.hpp"
#include "error_types/parsing/statements/err_stmt_return_creation_failed.hpp"
#include "error_types/parsing/statements/err_stmt_throw_creation_failed.hpp"
#include "error_types/parsing/statements/err_stmt_while_creation_failed.hpp"

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
inline void throw_err(ErrorType error_type, const char *file = __FILE__, const int line = __LINE__) {
    // throw std::runtime_error("Custom Error: " + std::to_string(static_cast<int>(error_type)));
    std::cerr << "Custom Error: " << std::to_string(static_cast<int>(error_type));
#ifdef DEBUG_BUILD
    std::cerr << "\n[Debug Info] Called from: " << file << ":" << line;
#endif
    std::cerr << std::endl;
}

#define THROW_BASIC_ERR(ErrorType) throw_err(ErrorType, __FILE__, __LINE__)

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
throw_err(const char *file = __FILE__, int line = __LINE__, Args &&...args) {
    ErrorType error(std::forward<Args>(args)...);
    std::cerr << error.to_string();
#ifdef DEBUG_BUILD
    std::cerr << "\n[Debug Info] Called from: " << file << ":" << line;
#endif
    std::cerr << std::endl;
    // std::exit(EXIT_FAILURE);
}

// Define a macro to autimatically pass file and line information
#define THROW_ERR(ErrorType, ...) throw_err<ErrorType>(__FILE__, __LINE__, ##__VA_ARGS__)

#endif
