#pragma once

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST USE STATEMENT ---
namespace {
    TestResult test_match_use_statement() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("USE_STATEMENT TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("USE_STATEMENT_MATCH:", true);
        return test_result;
    }

    TestResult test_match_use_statement_string() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_use_statement_string", false);
        token_list tokens = create_token_vector({//
            TOK_USE, TOK_STR_VALUE});
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_use_statement_package_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_use_statement_package_single", false);
        token_list tokens = create_token_vector({//
            TOK_USE, TOK_IDENTIFIER});
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_use_statement_package_dual() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_use_statement_package_dual", false);
        token_list tokens = create_token_vector({//
            TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER});
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_use_statement_package_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_use_statement_package_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER});
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_use_statement_flint_package_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_use_statement_flint_package_single", false);
        token_list tokens = create_token_vector({//
            TOK_USE, TOK_FLINT});
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_use_statement_flint_package_dual() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_use_statement_flint_package_dual", false);
        token_list tokens = create_token_vector({//
            TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER});
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_use_statement_flint_package_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_use_statement_flint_package_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER});
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST USE STATEMENT ---
namespace {
    TestResult test_contain_use_statement() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("USE_STATEMENT_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_use_statement_string() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_use_statement_string", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_STR_VALUE, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_use_statement_package_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_use_statement_package_single", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_use_statement_package_dual() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_use_statement_package_dual", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_use_statement_package_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_use_statement_package_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_use_statement_flint_package_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_use_statement_flint_package_single", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_FLINT, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_use_statement_flint_package_dual() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_use_statement_flint_package_dual", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_use_statement_flint_package_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_use_statement_flint_package_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST USE STATEMENT ---
namespace {
    TestResult test_extract_use_statement() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("USE_STATEMENT_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_use_statement_string() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_use_statement_string", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_STR_VALUE, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_use_statement_package_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_use_statement_package_single", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_use_statement_package_dual() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_use_statement_package_dual", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_use_statement_package_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_use_statement_package_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_use_statement_flint_package_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_use_statement_flint_package_single", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_FLINT, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_use_statement_flint_package_dual() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_use_statement_flint_package_dual", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_use_statement_flint_package_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_use_statement_flint_package_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_use_statement_tests() {
    function_list use_statement_tests = {
        // Match Tests
        test_match_use_statement,
        test_match_use_statement_string,
        test_match_use_statement_package_single,
        test_match_use_statement_package_dual,
        test_match_use_statement_package_multiple,
        test_match_use_statement_flint_package_single,
        test_match_use_statement_flint_package_dual,
        test_match_use_statement_flint_package_multiple,
        // Contain Tests
        test_contain_use_statement,
        test_contain_use_statement_string,
        test_contain_use_statement_package_single,
        test_contain_use_statement_package_dual,
        test_contain_use_statement_package_multiple,
        test_contain_use_statement_flint_package_single,
        test_contain_use_statement_flint_package_dual,
        test_contain_use_statement_flint_package_multiple,
        // Extract Tests
        test_extract_use_statement,
        test_extract_use_statement_string,
        test_extract_use_statement_package_single,
        test_extract_use_statement_package_dual,
        test_extract_use_statement_package_multiple,
        test_extract_use_statement_flint_package_single,
        test_extract_use_statement_flint_package_dual,
        test_extract_use_statement_flint_package_multiple,
    };
    return use_statement_tests;
}
