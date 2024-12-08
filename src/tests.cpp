#include "tests.hpp"
#include "test_utils.hpp"

#include "parser/test_parser.hpp"
#include "parser/test_signature.hpp"

TestResult run_parser_tests() {
    init_test();
    int test_sum = test_parser();
    end_test();
    return {test_output_buffer.c_str(), test_sum};
}

TestResult run_signature_tests() {
    init_test();
    int test_sum = test_signature();
    end_test();
    return {test_output_buffer.c_str(), test_sum};
}
