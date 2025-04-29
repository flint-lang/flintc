#pragma once

#include "debug.hpp"
#include "matcher/matcher.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST ARGS ---
namespace {
    TestResult test_match_args() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("ARGS TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("ARGS_MATCH:", true);
        return test_result;
    }

    TestResult test_match_args_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_args_single", false);
        token_list tokens = create_token_vector({//
            TOK_I32, TOK_IDENTIFIER});
        bool result = Matcher::tokens_match({tokens.begin(), tokens.end()}, Matcher::args);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_args_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_args_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER});
        bool result = Matcher::tokens_match({tokens.begin(), tokens.end()}, Matcher::args);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST ARGS ---
namespace {
    TestResult test_contain_args() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("ARGS_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_args_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_args_single", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Matcher::tokens_contain({tokens.begin(), tokens.end()}, Matcher::args);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_args_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_args_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN,
            TOK_COLON});
        bool result = Matcher::tokens_contain({tokens.begin(), tokens.end()}, Matcher::args);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST ARGS ---
namespace {
    TestResult test_extract_args() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("ARGS_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_args_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_args_single", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges({tokens.begin(), tokens.end()}, Matcher::args);
        bool result = !result_vec.empty() && result_vec.at(0).first == 3 && result_vec.at(0).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_args_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_args_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN,
            TOK_COLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges({tokens.begin(), tokens.end()}, Matcher::args);
        bool result = !result_vec.empty() && result_vec.at(0).first == 3 && result_vec.at(0).second == 8;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_args_tests() {
    function_list args_tests = {
        // Match Tests
        test_match_args,
        test_match_args_single,
        test_match_args_multiple,
        // Contain Tests
        test_contain_args,
        test_contain_args_single,
        test_contain_args_multiple,
        // Extract Tests
        test_extract_args,
        test_extract_args_single,
        test_extract_args_multiple,
    };
    return args_tests;
}
