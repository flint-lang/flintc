#ifndef __TESTS_HPP__
#define __TESTS_HPP__

#ifdef __cplusplus
extern "C" {
#endif

struct TestResult {
    const char* message;
    int count;
};

struct TestResult run_tests();

#ifdef __cplusplus
}
#endif

#endif
