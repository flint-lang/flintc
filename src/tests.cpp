#include "tests.hpp"
#include "test_utils.hpp"

#include "parser/test_parser.hpp"
#include "parser/test_signature.hpp"

TestResult run_parser_tests() {
    TestUtils::init_test();
    int test_sum = test_parser();
    TestUtils::end_test();
    return {TestUtils::get_output().c_str(), test_sum};
}

TestResult run_signature_tests() {
    TestUtils::init_test();
    int test_sum = test_signature();
    TestUtils::end_test();
    return {TestUtils::get_output().c_str(), test_sum};
}
