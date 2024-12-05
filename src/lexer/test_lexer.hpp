#ifndef __TEST_LEXER_HPP__
#define __TEST_LEXER_HPP__

#include "../test_utils.hpp"

#include <string>
#include <vector>

namespace {

}

int test_lexer() {
    print_test_name(0, "LEXER_TESTS:", true);
    print_test_name(1, "SOME_TESTS:", true);

    function_list some_tests = {

    };

    const std::vector<function_list> tests = {
        some_tests,
    };
    return run_all_tests(tests);
}

#endif
