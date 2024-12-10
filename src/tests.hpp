#ifndef __TESTS_HPP__
#define __TESTS_HPP__

#ifdef __cplusplus
extern "C" {
#endif

struct TestResult {
    const wchar_t* message;
    int count;
};

struct TestResult run_parser_tests();
struct TestResult run_signature_tests();

#ifdef __cplusplus
}
#endif

#endif
