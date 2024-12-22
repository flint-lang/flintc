#ifndef __TEST_SIGNATURE_HPP__
#define __TEST_SIGNATURE_HPP__

#include "test_utils.hpp"

#include "balanced_range_extraction_tests.hpp"
#include "balanced_range_extraction_vec_tests.hpp"

#include "basics/args_tests.hpp"
#include "basics/group_tests.hpp"
#include "basics/primary_tests.hpp"
#include "basics/reference_tests.hpp"
#include "basics/type_tests.hpp"

#include "definitions/data_definition_tests.hpp"
#include "definitions/entity_definition_tests.hpp"
#include "definitions/enum_definition_tests.hpp"
#include "definitions/error_definition_tests.hpp"
#include "definitions/func_definition_tests.hpp"
#include "definitions/function_definition_tests.hpp"
#include "definitions/use_statement_tests.hpp"
#include "definitions/variant_definition_tests.hpp"

#include "expressions/bin_op_expression_tests.hpp"
#include "expressions/function_call_expression_tests.hpp"

#include <string>
#include <vector>

TestResult test_signature() {
    TestResult result;
    result.append_test_name("SIGNATURE_TESTS:", true);

    const std::vector<function_list> tests = {
        // --- SIGNATURE METHODS ---
        get_balanced_range_extraction_tests(),
        get_balanced_range_extraction_vec_tests(),
        // --- BASIC SIGNATURES ---
        get_primary_tests(),
        get_type_tests(),
        get_reference_tests(),
        get_args_tests(),
        get_group_tests(),
        // --- DEFINITIONS ---
        get_use_statement_tests(),
        get_function_definition_tests(),
        get_data_definition_tests(),
        get_func_definition_tests(),
        get_entity_definition_tests(),
        get_error_definition_tests(),
        get_enum_definition_tests(),
        get_variant_definition_tests(),
        // --- EXPRESSIONS ---
        get_function_call_expression_tests(),
        get_bin_op_expression_tests(),
    };
    run_all_tests(result, tests, false);

    if (result.get_count() == 0) {
        return {};
    }
    return result;
}

#endif
