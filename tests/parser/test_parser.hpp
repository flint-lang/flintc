#pragma once

#include "test_utils.hpp"

TestResult test_parser() {
    TestResult result;
    result.append_test_name("PARSER_TESTS:", true);

    if (result.get_count() == 0) {
        return {};
    }
    return result;
}
