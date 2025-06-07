#pragma once

#include "colors.hpp"
#include "error_type.hpp"
#include "globals.hpp"

// All error types are included here to make using the error header file easier
#include "error_types/base_error.hpp"

// --- LEXING ERRORS ---
#include "error_types/lexing/comments/err_comment_unterm_multiline.hpp"
#include "error_types/lexing/identifiers/err_invalid_identifier.hpp"
#include "error_types/lexing/literals/err_lit_char_longer_than_single_character.hpp"
#include "error_types/lexing/literals/err_lit_expected_char_value.hpp"
#include "error_types/lexing/literals/err_lit_unterminated_string.hpp"
#include "error_types/lexing/unexpected/err_unexpected_token.hpp"
#include "error_types/lexing/unexpected/err_unexpected_token_number.hpp"
#include "error_types/lexing/unexpected/err_unexpected_token_pipe.hpp"

// --- PARSING ERRORS ---
#include "error_types/parsing/definitions/data/err_def_data_creation.hpp"
#include "error_types/parsing/definitions/data/err_def_data_duplicate_field_name.hpp"
#include "error_types/parsing/definitions/data/err_def_data_redefinition.hpp"
#include "error_types/parsing/definitions/data/err_def_data_wrong_constructor_name.hpp"
#include "error_types/parsing/definitions/entity/err_def_entity_wrong_constructor_name.hpp"
#include "error_types/parsing/definitions/err_def_err_only_one_parent.hpp"
#include "error_types/parsing/definitions/err_def_no_main_function.hpp"
#include "error_types/parsing/definitions/err_unexpected_definition.hpp"
#include "error_types/parsing/definitions/func/err_def_func_creation.hpp"
#include "error_types/parsing/definitions/function/err_def_function_creation.hpp"
#include "error_types/parsing/definitions/import//err_alias_not_found.hpp"
#include "error_types/parsing/definitions/import/err_def_not_found_in_aliased_file.hpp"
#include "error_types/parsing/definitions/import/err_def_unexpected_core_module.hpp"
#include "error_types/parsing/definitions/import/err_use_statement_not_at_top_level.hpp"
#include "error_types/parsing/definitions/test/err_test_redefinition.hpp"

#include "error_types/parsing/expressions/err_expr_binop_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_binop_type_mismatch.hpp"
#include "error_types/parsing/expressions/err_expr_call_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_call_of_undefined_function.hpp"
#include "error_types/parsing/expressions/err_expr_call_wrong_arg_count.hpp"
#include "error_types/parsing/expressions/err_expr_call_wrong_args_builtin.hpp"
#include "error_types/parsing/expressions/err_expr_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_lit_creation_failed.hpp"
#include "error_types/parsing/expressions/err_expr_nested_group.hpp"
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

#include "error_types/parsing/unexpected/err_pars_unexpected_token.hpp"

#include "error_types/parsing/values/err_val_unknown_literal.hpp"

#include "error_types/parsing/variables/err_var_from_requires_list.hpp"
#include "error_types/parsing/variables/err_var_mutating_const.hpp"
#include "error_types/parsing/variables/err_var_not_declared.hpp"
#include "error_types/parsing/variables/err_var_redefinition.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <type_traits>

/// @brief Throws the given ErrorType as an runtime error to the console. Very basic error handling
///
/// @param error_type The error enum type, whose Enum ID will be printed to the console
inline void throw_err(ErrorType error_type, const char *file = __FILE__, const int line = __LINE__) {
    std::cerr << "Custom Error: " << std::to_string(static_cast<int>(error_type));
    if (DEBUG_MODE) {
        std::cerr << YELLOW << "\n[Debug Info]" << DEFAULT << " Called from: " << file << ":" << line;
    }
    if (HARD_CRASH) {
        assert(false);
    }
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
    if (DEBUG_MODE) {
        std::cerr << YELLOW << "\n[Debug Info]" << DEFAULT << " Called from: " << file << ":" << line;
    }
    if (HARD_CRASH) {
        assert(false);
    }
    std::cerr << "\n" << std::endl;
}

// Define a macro to autimatically pass file and line information
#define THROW_ERR(ErrorType, ...) throw_err<ErrorType>(__FILE__, __LINE__, ##__VA_ARGS__)
