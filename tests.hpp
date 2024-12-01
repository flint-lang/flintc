#ifndef __TESTS_HPP__
#define __TESTS_HPP__

#include "parser/test_parser.hpp"
#include "test_utils.hpp"

int test_all() {
    init_test();

    int test_sum = 0;
    test_sum += test_parser();

    return test_sum;
}

#endif
