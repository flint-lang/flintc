#pragma once

#include "test_utils.hpp"

#include <string>
#include <vector>

TestResult test_lexer() {
    TestResult result;
    result.append_test_name("LEXER_TESTS:", true);

    function_list some_tests = {

    };

    const std::vector<function_list> tests = {
        some_tests,
    };
    run_all_tests(result, tests, true);
    return result;
}
