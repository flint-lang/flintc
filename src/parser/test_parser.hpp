#ifndef __TEST_PARSER_HPP__
#define __TEST_PARSER_HPP__

#include "test_signature.hpp"

int test_parser() {
    print_test_name(0, "PARSER_TESTS:", true);

    return test_signature();
}

#endif
