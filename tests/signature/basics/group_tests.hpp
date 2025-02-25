#ifndef __GROUP_TESTS_HPP__
#define __GROUP_TESTS_HPP__

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST GROUP ---
namespace {
    TestResult test_match_group() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("GROUP TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("GROUP_MATCH:", true);
        return test_result;
    }

    TestResult test_match_group_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_group_single", false);
        token_list tokens = create_token_vector({//
            TOK_LEFT_PAREN, TOK_I32, TOK_RIGHT_PAREN});
        bool result = Signature::tokens_match(tokens, Signature::group);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_group_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_group_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN});
        bool result = Signature::tokens_match(tokens, Signature::group);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST GROUP ---
namespace {
    TestResult test_contain_group() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("GROUP_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_group_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_group_single", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_I32,
            TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Signature::tokens_contain(tokens, Signature::group);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_group_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_group_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN,
            TOK_ARROW, TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Signature::tokens_contain(tokens, Signature::group);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST GROUP ---
namespace {
    TestResult test_extract_group() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("GROUP_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_group_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_group_single", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_I32,
            TOK_RIGHT_PAREN, TOK_COLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::group);
        bool result = !result_vec.empty() && result_vec.at(0).first == 7 && result_vec.at(0).second == tokens.size() - 1;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_group_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_group_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN,
            TOK_ARROW, TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::group);
        bool result = !result_vec.empty() && result_vec.at(0).first == 10 && result_vec.at(0).second == 15;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_group_tests() {
    function_list group_tests = {
        // Match Tests
        test_match_group,
        test_match_group_single,
        test_match_group_multiple,
        // Contain Tests
        test_contain_group,
        test_contain_group_single,
        test_contain_group_multiple,
        // Extract Tests
        test_extract_group,
        test_extract_group_single,
        test_extract_group_multiple,
    };
    return group_tests;
}

#endif
