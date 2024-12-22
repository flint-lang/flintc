#ifndef __REFERENCE_TESTS_HPP__
#define __REFERENCE_TESTS_HPP__

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST REFERENCE ---
namespace {
    TestResult test_match_reference() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("REFERENCE TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("REFERENCE_MATCH:", true);
        return test_result;
    }

    TestResult test_match_reference_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_reference_single", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER});
        bool result = Signature::tokens_match(tokens, Signature::reference);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_reference_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_reference_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER});
        bool result = Signature::tokens_match(tokens, Signature::reference);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST REFERENCE ---
namespace {
    TestResult test_contain_reference() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("REFERENCE_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_reference_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_reference_single", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::reference);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_reference_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_reference_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON});
        bool result = Signature::tokens_contain(tokens, Signature::reference);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST REFERENCE ---
namespace {
    TestResult test_extract_reference() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("REFERENCE_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_reference_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_reference_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::reference);
        bool result = !result_vec.empty() && result_vec.at(0).first == 4 && result_vec.at(0).second == tokens.size() - 1;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_reference_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_reference_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON,
            TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::reference);
        bool result = !result_vec.empty() && result_vec.at(0).first == 4 && result_vec.at(0).second == tokens.size() - 1;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_reference_tests() {
    function_list reference_tests = {
        // Match Tests Reference
        test_match_reference,
        test_match_reference_single,
        test_match_reference_multiple,
        // Contain Tests Reference
        test_contain_reference,
        test_contain_reference_single,
        test_contain_reference_multiple,
        // Extract Tests Reference
        test_extract_reference,
        test_extract_reference_single,
        test_extract_reference_multiple,
    };
    return reference_tests;
}

#endif
