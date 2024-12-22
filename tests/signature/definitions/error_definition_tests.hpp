#ifndef __ERROR_DEFINITION_TESTS_HPP__
#define __ERROR_DEFINITION_TESTS_HPP__

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST ERROR DEFINITION ---
namespace {
    TestResult test_match_error_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("ERROR_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("ERROR_DEFINITION_MATCH:", true);
        return test_result;
    }

    TestResult test_match_error_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_error_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_ERROR, TOK_IDENTIFIER, TOK_COLON});
        bool result = Signature::tokens_match(tokens, Signature::error_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_error_definition_extending() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_error_definition_extending", false);
        token_list tokens = create_token_vector({//
            TOK_ERROR, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Signature::tokens_match(tokens, Signature::error_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST ERROR DEFINITION ---
namespace {
    TestResult test_contain_error_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("ERROR_DEFINITION_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_error_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_error_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ERROR, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, Signature::error_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_error_definition_extending() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_error_definition_extending", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ERROR, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, Signature::error_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST ERROR DEFINITION ---
namespace {
    TestResult test_extract_error_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("ERROR_DEFINITION_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_error_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_error_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ERROR, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::error_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_error_definition_extending() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_error_definition_extending", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ERROR, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::error_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_error_definition_tests() {
    function_list error_definition_tests = {
        // Match Tests
        test_match_error_definition,
        test_match_error_definition_normal,
        test_match_error_definition_extending,
        // Contain Tests
        test_contain_error_definition,
        test_contain_error_definition_normal,
        test_contain_error_definition_extending,
        // Extract Tests
        test_extract_error_definition,
        test_extract_error_definition_normal,
        test_extract_error_definition_extending,
    };
    return error_definition_tests;
}

#endif
