#pragma once

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST BINARY OPERATOR EXPRESSION ---
namespace {
    TestResult test_match_bin_op_expr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::SINGLE}, &test_result);
        test_result.append_test_name("BINARY_OPERATOR_EXPRESSION TESTS:", true);
        Debug::print_tree_row({Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("BIN_OP_EXPR_MATCH:", true);
        return test_result;
    }

    TestResult test_match_bin_op_expr_vars_square_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::NONE, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_bin_op_expr_vars_square", false);
        // 4 ** 5
        token_list tokens = create_token_vector({//
            TOK_INT_VALUE, TOK_SQUARE, TOK_INT_VALUE});
        bool result = Signature::tokens_match(tokens, Signature::bin_op_expr);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST BINARY OPERATOR EXPRESSION ---
namespace {
    TestResult test_contain_bin_op_expr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("BIN_OP_EXPR_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_bin_op_expr_vars_square_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::NONE, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_bin_op_expr_vars_square_int", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_EQUAL, TOK_INT_VALUE, TOK_SQUARE, TOK_INT_VALUE, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::bin_op_expr);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST BINARY OPERATOR EXPRESSION ---
namespace {
    TestResult test_extract_bin_op_expr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("BIN_OP_EXPR_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_bin_op_expr_vars_square_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::NONE, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_bin_op_expr_vars_square_int", false);
        token_list tokens = create_token_vector({//
            TOK_INT_VALUE, TOK_SQUARE, TOK_INT_VALUE, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::bin_op_expr);
        bool result = !result_vec.empty() && result_vec.at(0).first == 0 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_bin_op_expression_tests() {
    function_list bin_op_expr_tests = {
        // Match Tests
        test_match_bin_op_expr,
        test_match_bin_op_expr_vars_square_int,
        // Contain Tests
        test_contain_bin_op_expr,
        test_contain_bin_op_expr_vars_square_int,
        // Extract Tests
        test_extract_bin_op_expr,
        test_extract_bin_op_expr_vars_square_int,
    };
    return bin_op_expr_tests;
}
