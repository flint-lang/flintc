#ifndef __TEST_PERFORMANCE_HPP__
#define __TEST_PERFORMANCE_HPP__

#include "test_utils.hpp"
#include <string>

TestResult test_performance(const std::string &flags, const unsigned int count) {
    TestResult result;
    result.append_test_name("PERFORMANCE_TESTS:", true);
    std::cout << "Testing with flags \"" << flags << "\", running each test " << count << " times\n\n";

    run_performance_test("tests/performance/recursion/factorial", flags, count);
    run_performance_test("tests/performance/recursion/fibonacci", flags, count);

    run_performance_test("tests/performance/loops/calculations_in_loop", flags, count);
    run_performance_test("tests/performance/loops/prints_in_loop", flags, count);

    return result;
}

#endif
