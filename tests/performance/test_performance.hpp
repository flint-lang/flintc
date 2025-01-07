#ifndef __TEST_PERFORMANCE_HPP__
#define __TEST_PERFORMANCE_HPP__

#include "test_utils.hpp"

TestResult test_performance() {
    TestResult result;
    result.append_test_name("PERFORMANCE_TESTS:", true);

    run_performance_test("tests/performance/recursion/factorial", "");
    run_performance_test("tests/performance/recursion/fibonacci", "");

    return result;
}

#endif
