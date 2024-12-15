#ifndef __TEST_LEXER_HPP__
#define __TEST_LEXER_HPP__

#include "../test_utils.hpp"

#include <string>
#include <vector>

namespace {}

int test_lexer() {
    TestUtils::print_test_name("LEXER_TESTS:", true);
    TestUtils::print_test_name("SOME_TESTS:", true);

    function_list some_tests = {

    };

    const std::vector<function_list> tests = {
        some_tests,
    };
    return run_all_tests(tests);
}

#endif
