#pragma once

#include "colors.hpp"
#include "diagnostics.hpp" // IWYU pragma: keep
#include "error_type.hpp"
#include "globals.hpp"

// All error types are included here to make using the error header file easier
#include "error_types/base_error.hpp"

// --- LEXING ERRORS ---
#include "error_types/lexing/comments/err_comment_unterm_multiline.hpp"              // IWYU pragma: keep
#include "error_types/lexing/literals/err_lit_char_longer_than_single_character.hpp" // IWYU pragma: keep
#include "error_types/lexing/literals/err_lit_expected_char_value.hpp"               // IWYU pragma: keep
#include "error_types/lexing/literals/err_lit_int_too_large.hpp"                     // IWYU pragma: keep
#include "error_types/lexing/literals/err_lit_int_too_small.hpp"                     // IWYU pragma: keep
#include "error_types/lexing/literals/err_lit_unterminated_string.hpp"               // IWYU pragma: keep
#include "error_types/lexing/unexpected/err_unexpected_token.hpp"                    // IWYU pragma: keep
#include "error_types/lexing/unexpected/err_unexpected_token_number.hpp"             // IWYU pragma: keep
#include "error_types/lexing/unexpected/err_unexpected_token_pipe.hpp"               // IWYU pragma: keep

// --- PARSING ERRORS ---
#include "error_types/parsing/annotations/err_anno_duplicate.hpp" // IWYU pragma: keep
#include "error_types/parsing/annotations/err_anno_leftover.hpp"  // IWYU pragma: keep
#include "error_types/parsing/annotations/err_anno_unknown.hpp"   // IWYU pragma: keep

#include "error_types/parsing/definitions/data/err_def_data_duplicate_field_name.hpp"               // IWYU pragma: keep
#include "error_types/parsing/definitions/data/err_def_data_wrong_constructor_name.hpp"             // IWYU pragma: keep
#include "error_types/parsing/definitions/err_def_err_only_one_parent.hpp"                          // IWYU pragma: keep
#include "error_types/parsing/definitions/err_def_no_main_function.hpp"                             // IWYU pragma: keep
#include "error_types/parsing/definitions/err_def_redefinition.hpp"                                 // IWYU pragma: keep
#include "error_types/parsing/definitions/err_unexpected_definition.hpp"                            // IWYU pragma: keep
#include "error_types/parsing/definitions/func/err_def_func_contains_virtual_function.hpp"          // IWYU pragma: keep
#include "error_types/parsing/definitions/func/err_def_func_required_type_not_data.hpp"             // IWYU pragma: keep
#include "error_types/parsing/definitions/func/err_def_func_required_type_unknown.hpp"              // IWYU pragma: keep
#include "error_types/parsing/definitions/func/err_def_func_requiring_same_data_twice.hpp"          // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_cannot_return_tuple.hpp"                  // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_comptime_param_type.hpp"                  // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_comptime_return_type.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_def_missing.hpp"                          // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_main_err_set.hpp"                         // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_main_no_returns.hpp"                      // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_main_redefinition.hpp"                    // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_main_too_many_args.hpp"                   // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_main_wrong_arg_type.hpp"                  // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_param_shadows_required_data.hpp"          // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_redefinition.hpp"                         // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_reserved_name.hpp"                        // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_specialization_failed.hpp"                // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_specialize_param_type_failed.hpp"         // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_void_in_return_group.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/definitions/function/err_fn_void_param_type.hpp"                      // IWYU pragma: keep
#include "error_types/parsing/definitions/import/err_import_duplicate_alias.hpp"                    // IWYU pragma: keep
#include "error_types/parsing/definitions/import/err_import_exited_cwd.hpp"                         // IWYU pragma: keep
#include "error_types/parsing/definitions/import/err_import_nonexistent_file.hpp"                   // IWYU pragma: keep
#include "error_types/parsing/definitions/import/err_import_not_at_top_level.hpp"                   // IWYU pragma: keep
#include "error_types/parsing/definitions/import/err_import_same_file_twice.hpp"                    // IWYU pragma: keep
#include "error_types/parsing/definitions/import/err_import_unexpected_core_module.hpp"             // IWYU pragma: keep
#include "error_types/parsing/definitions/interface/err_def_func_contains_virtual_function.hpp"     // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_duplicate_accessor.hpp"             // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_duplicate_data.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_duplicate_func.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_duplicate_interface.hpp"            // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_implemented_type_not_interface.hpp" // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_implemented_type_unknown.hpp"       // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_missing_data.hpp"                   // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_no_data.hpp"                        // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_no_functionality.hpp"               // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_provided_type_not_data.hpp"         // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_provided_type_not_func.hpp"         // IWYU pragma: keep
#include "error_types/parsing/definitions/object/err_def_object_unresolved_virtual.hpp"             // IWYU pragma: keep
#include "error_types/parsing/definitions/test/err_test_entry_duplicate.hpp"                        // IWYU pragma: keep
#include "error_types/parsing/definitions/test/err_test_entry_invalid.hpp"                          // IWYU pragma: keep
#include "error_types/parsing/definitions/test/err_test_entry_on_main.hpp"                          // IWYU pragma: keep
#include "error_types/parsing/definitions/test/err_test_redefinition.hpp"                           // IWYU pragma: keep
#include "error_types/parsing/definitions/test/err_test_setup_duplicate.hpp"                        // IWYU pragma: keep

#include "error_types/parsing/expressions/err_expr_array_access_not_allowed_on_type.hpp"            // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_array_needs_initializer.hpp"                     // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_call_ambiguous.hpp"                              // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_call_missing_closing_paren.hpp"                  // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_call_of_undefined_function.hpp"                  // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_call_of_virtual_function.hpp"                    // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_call_on_const_instance.hpp"                      // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_call_on_wrong_instance_type.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_cast_vector_length_mismatch.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_data_initializer_missing_default_value.hpp"      // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_enum_tag_not_present.hpp"                        // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_field_access_not_allowed_on_type.hpp"            // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_field_access_on_object.hpp"                      // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_field_nonexistent.hpp"                           // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_fn_ref_core.hpp"                                 // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_fn_ref_nonexistent.hpp"                          // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_initializer_duplicate_field.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_initializer_field_mixed_styles.hpp"              // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_initializer_field_not_default_constructible.hpp" // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_initializer_field_wrong_format.hpp"              // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_initializer_too_many_values.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_initializer_wrong_arg_count.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_interpolation_only_one_expr.hpp"                 // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_mutating_const.hpp"                              // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_nested_group.hpp"                                // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_not_recognizable.hpp"                            // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_tuple_access_oob.hpp"                            // IWYU pragma: keep
#include "error_types/parsing/expressions/err_expr_unary_op_missing_expr.hpp"                       // IWYU pragma: keep

#include "error_types/parsing/fip/err_ambiguous_module_tag.hpp"   // IWYU pragma: keep
#include "error_types/parsing/fip/err_extern_fn_not_found.hpp"    // IWYU pragma: keep
#include "error_types/parsing/fip/err_extern_without_fip.hpp"     // IWYU pragma: keep
#include "error_types/parsing/fip/err_no_fip_directory_found.hpp" // IWYU pragma: keep
#include "error_types/parsing/fip/err_unknown_module_tag.hpp"     // IWYU pragma: keep
#include "error_types/parsing/fip/err_use_without_fip.hpp"        // IWYU pragma: keep

#include "error_types/parsing/scopes/err_missing_body.hpp"      // IWYU pragma: keep
#include "error_types/parsing/scopes/err_missing_colon.hpp"     // IWYU pragma: keep
#include "error_types/parsing/scopes/err_missing_semicolon.hpp" // IWYU pragma: keep

#include "error_types/parsing/statements/err_stmt_dangling_catch.hpp"                    // IWYU pragma: keep
#include "error_types/parsing/statements/err_stmt_dangling_else.hpp"                     // IWYU pragma: keep
#include "error_types/parsing/statements/err_stmt_if_chain_missing_if.hpp"               // IWYU pragma: keep
#include "error_types/parsing/statements/err_stmt_mussing_initializer_of_freeable.hpp"   // IWYU pragma: keep
#include "error_types/parsing/statements/err_stmt_mussing_initializer_of_persistent.hpp" // IWYU pragma: keep
#include "error_types/parsing/statements/err_stmt_not_recognizable.hpp"                  // IWYU pragma: keep

#include "error_types/parsing/types/err_type_not_default_constructible.hpp"        // IWYU pragma: keep
#include "error_types/parsing/types/err_type_tuple_vector_overlap.hpp"             // IWYU pragma: keep
#include "error_types/parsing/types/err_type_unknown.hpp"                          // IWYU pragma: keep
#include "error_types/parsing/types/err_type_unknown_for_default_construction.hpp" // IWYU pragma: keep

#include "error_types/parsing/unexpected/err_pars_unexpected_token.hpp" // IWYU pragma: keep

#include "error_types/parsing/variables/err_var_from_requires_list.hpp" // IWYU pragma: keep
#include "error_types/parsing/variables/err_var_mutating_const.hpp"     // IWYU pragma: keep
#include "error_types/parsing/variables/err_var_not_declared.hpp"       // IWYU pragma: keep
#include "error_types/parsing/variables/err_var_redefinition.hpp"       // IWYU pragma: keep

#include "error_types/parsing/err_call_of_comptime_only_function.hpp"      // IWYU pragma: keep
#include "error_types/parsing/err_call_of_extern_function_at_comptime.hpp" // IWYU pragma: keep

// --- GENERATING ERRORS ---
#include "error_types/generating/fip/err_extern_compilation_failed.hpp" // IWYU pragma: keep
#include "error_types/generating/fip/err_extern_duplicate_function.hpp" // IWYU pragma: keep

// --- ANALYZING ERRORS ---
#include "error_types/analyzing/err_empty_stored_fixed_array.hpp"                        // IWYU pragma: keep
#include "error_types/analyzing/err_expr_binop_type_mismatch.hpp"                        // IWYU pragma: keep
#include "error_types/analyzing/err_expr_cast_invalid.hpp"                               // IWYU pragma: keep
#include "error_types/analyzing/err_expr_type_mismatch.hpp"                              // IWYU pragma: keep
#include "error_types/analyzing/err_ptr_not_allowed_in_internal_function_definition.hpp" // IWYU pragma: keep
#include "error_types/analyzing/err_ptr_not_allowed_in_non_extern_context.hpp"           // IWYU pragma: keep

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
    std::cerr << std::endl;
    if (HARD_CRASH) {
        ASSERT(false);
    }
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
throw_err(                                                                                         //
    [[maybe_unused]] const char *file = __FILE__,                                                  //
    [[maybe_unused]] int line = __LINE__,                                                          //
    Args &&...args                                                                                 //
) {
    ErrorType error(std::forward<Args>(args)...);
#ifdef FLINT_LSP
    diagnostics.emplace_back(error.to_diagnostic());
#else
    std::cerr << error.to_string();
    if (DEBUG_MODE) {
        std::cerr << YELLOW << "\n[Debug Info]" << DEFAULT << " Called from: " << file << ":" << line;
    }
    std::cerr << "\n" << std::endl;
    if (HARD_CRASH) {
        ASSERT(false);
    }
#endif
}

// Define a macro to autimatically pass file and line information
#define THROW_ERR(ErrorType, ...) throw_err<ErrorType>(__FILE__, __LINE__, ##__VA_ARGS__)
