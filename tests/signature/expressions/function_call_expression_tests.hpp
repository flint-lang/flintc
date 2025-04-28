#pragma once

#include "debug.hpp"
#include "matcher/matcher.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST FUNCTION CALL EXPRESSION ---
namespace {
    TestResult test_match_function_call() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNCTION_CALL TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNCTION_CALL_MATCH:", true);
        return test_result;
    }

    TestResult test_match_function_call_0arg() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_call_0arg", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN});
        bool result = Matcher::tokens_match(tokens, Matcher::function_call);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_call_1arg_identifier() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_call_1arg_identifier", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN});
        bool result = Matcher::tokens_match(tokens, Matcher::function_call);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_call_1arg_function_0arg() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_function_call_1arg_function_0arg", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN});
        bool result = Matcher::tokens_match(tokens, Matcher::function_call);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST FUNCTION CALL EXPRESSION ---
namespace {
    TestResult test_contain_function_call() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNCTION_CALL_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_function_call_0arg() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_call_0arg", false);
        token_list tokens = create_token_vector({//
            TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_call);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_call_1arg_identifier() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_call_1arg_identifier", false);
        token_list tokens = create_token_vector({//
            TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_call);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_call_1arg_function_0arg() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_function_call_1arg_function_0arg", false);
        token_list tokens = create_token_vector(
            {TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_call);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST FUNCTION CALL EXPRESSION ---
namespace {
    TestResult test_extract_function_call() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("FUNCTION_CALL_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_function_call_0arg() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_call_0arg", false);
        token_list tokens = create_token_vector({//
            TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_call);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_call_1arg_identifier() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_call_1arg_identifier", false);
        token_list tokens = create_token_vector({//
            TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_call);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_call_1arg_function_0arg() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_function_call_1arg_function_0arg", false);
        token_list tokens = create_token_vector(
            {TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_call);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_function_call_expression_tests() {
    function_list function_call_expression_tests = {
        // Match Tests
        test_match_function_call,
        test_match_function_call_0arg,
        test_match_function_call_1arg_identifier,
        test_match_function_call_1arg_function_0arg,
        // Contain Tests
        test_contain_function_call,
        test_contain_function_call_0arg,
        test_contain_function_call_1arg_identifier,
        test_contain_function_call_1arg_function_0arg,
        // Extract Tests
        test_extract_function_call,
        test_extract_function_call_0arg,
        test_extract_function_call_1arg_identifier,
        test_extract_function_call_1arg_function_0arg,
    };
    return function_call_expression_tests;
}
