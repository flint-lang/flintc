#include "test_utils.hpp"

#include "cli_parser_tests.hpp"
#include "fuzzy/test_fuzzy.hpp"
#include "parser/test_parser.hpp"
#include "performance/test_performance.hpp"
#include "signature/test_signature.hpp"

#include <iostream>

void print_result(const TestResult &result) {
    if (result.get_count() == 0) {
        std::cout << " --- All Tests Passed ---" << std::endl;
        return;
    }
    std::cout << " --- " << result.get_count() << " Test(s) Failed ---" << std::endl;
    std::cout << result.get_message() << std::endl;
}

int main(int argc, char *argv[]) {
    CLIParserTests clp(argc, argv);
    int cli_result = clp.parse();
    if (cli_result != 0) {
        return cli_result;
    }

    if (clp.unit_tests) {
        TestResult result;
        run_test(result, test_parser);
        run_test(result, test_signature);
        print_result(result);
        std::cout << DEFAULT;
    }

    if (clp.test_performance) {
        test_performance(clp.compile_flags, clp.count);
    }

    if (clp.fuzzy_testing) {
        test_fuzzy(clp.fuzzy_count);
    }
}
