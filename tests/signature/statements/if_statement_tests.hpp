#pragma once

#include "debug.hpp"
#include "lexer/token.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST IF STATEMENT ---
namespace {
    TestResult test_match_if() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("IF_STATEMENT TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("IF_STATEMENT_MATCH:", true);
        return test_result;
    }

    TestResult test_match_if_true() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_true", false);
        // if true:
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_TRUE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_false() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_false", false);
        // if false:
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_FALSE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_var_less_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_var_less_int", false);
        // if x < 4:
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_IDENTIFIER, TOK_LESS, TOK_INT_VALUE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_var_greater_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_var_greater_int", false);
        // if x > 10:
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_IDENTIFIER, TOK_GREATER, TOK_INT_VALUE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_var_less_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_var_less_eq_int", false);
        // if x <= 3:
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_IDENTIFIER, TOK_LESS_EQUAL, TOK_INT_VALUE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_var_greater_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_var_greater_eq_int", false);
        // if x >= 7:
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_IDENTIFIER, TOK_GREATER_EQUAL, TOK_INT_VALUE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_var_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_var_eq_int", false);
        // if x == 4:
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_IDENTIFIER, TOK_EQUAL_EQUAL, TOK_INT_VALUE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_var_neq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_var_neq_int", false);
        // if x != 5:
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_IDENTIFIER, TOK_NOT_EQUAL, TOK_INT_VALUE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_var_eq_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_var_eq_fn", false);
        // if x == func():
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_IDENTIFIER, TOK_EQUAL_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_if_fn", false);
        // if func():
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_if_not_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_if_fn", false);
        // if not func():
        token_list tokens = create_token_vector({//
            TOK_IF, TOK_NOT, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST IF STATEMENT ---
namespace {
    TestResult test_contain_if() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("IF_STATEMENT_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_if_true() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_true", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_TRUE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_false() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_false", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_FALSE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_var_less_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_var_less_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_LESS, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_var_greater_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_var_greater_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_GREATER, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_var_less_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_var_less_eq_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_LESS_EQUAL, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_var_greater_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_var_greater_eq_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_GREATER_EQUAL, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_var_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_var_eq_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_EQUAL_EQUAL, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_var_neq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_var_neq_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_NOT_EQUAL, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_var_eq_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_var_eq_fn", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_EQUAL_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_if_fn", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_if_not_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_if_not_fn", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_NOT, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::IF_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST IF STATEMENT ---
namespace {
    TestResult test_extract_if() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("IF_STATEMENT_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_if_true() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_true", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_TRUE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_false() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_false", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_FALSE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_var_less_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_var_less_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_LESS, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_var_greater_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_var_greater_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_GREATER, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_var_less_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_var_less_eq_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_LESS_EQUAL, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_var_greater_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_var_greater_eq_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_EQUAL_EQUAL, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_var_eq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_var_eq_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_EQUAL_EQUAL, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_var_neq_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_var_neq_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_NOT_EQUAL, TOK_INT_VALUE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_var_eq_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_var_eq_fn", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_EQUAL_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_if_fn", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_if_not_fn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_if_not_fn", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IF, TOK_NOT, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::IF_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_if_statement_tests() {
    function_list if_statement_tests = {
        // Match Tests
        test_match_if,
        test_match_if_true,
        test_match_if_false,
        test_match_if_var_less_int,
        test_match_if_var_greater_int,
        test_match_if_var_less_eq_int,
        test_match_if_var_greater_eq_int,
        test_match_if_var_eq_int,
        test_match_if_var_neq_int,
        test_match_if_var_eq_fn,
        test_match_if_fn,
        test_match_if_not_fn,
        // Contain Tests
        test_contain_if,
        test_contain_if_true,
        test_contain_if_false,
        test_contain_if_var_less_int,
        test_contain_if_var_greater_int,
        test_contain_if_var_less_eq_int,
        test_contain_if_var_greater_eq_int,
        test_contain_if_var_eq_int,
        test_contain_if_var_neq_int,
        test_contain_if_var_eq_fn,
        test_contain_if_fn,
        test_contain_if_not_fn,
        // Extract Tests
        test_extract_if,
        test_extract_if_true,
        test_extract_if_false,
        test_extract_if_var_less_int,
        test_extract_if_var_greater_int,
        test_extract_if_var_less_eq_int,
        test_extract_if_var_greater_eq_int,
        test_extract_if_var_eq_int,
        test_extract_if_var_neq_int,
        test_extract_if_var_eq_fn,
        test_extract_if_fn,
        test_extract_if_not_fn,
    };
    return if_statement_tests;
}
