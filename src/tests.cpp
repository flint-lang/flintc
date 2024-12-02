#include "parser/test_parser.hpp"
#include "test_utils.hpp"
#include "tests.hpp"

TestResult run_tests() {
    init_test();

    int test_sum = 0;
    test_sum += test_parser();

    return {test_output_buffer.c_str(), test_sum};
}
