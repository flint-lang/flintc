#pragma once

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST FUNC DEFINITION ---
namespace {
    TestResult test_match_func_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNC_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNC_DEFINITION_MATCH:", true);
        return test_result;
    }

    TestResult test_match_func_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_func_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_FUNC, TOK_IDENTIFIER, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::FUNC_DEFINITION);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_func_definition_requires_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_func_definition_requires_single", false);
        token_list tokens = create_token_vector(
            {TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::FUNC_DEFINITION);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_func_definition_requires_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_func_definition_requires_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER,
            TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::FUNC_DEFINITION);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST FUNC DEFINITION ---
namespace {
    TestResult test_contain_func_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNC_DEFINITION_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_func_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_func_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::FUNC_DEFINITION);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_func_definition_requires_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_func_definition_requires_single", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON,
            TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::FUNC_DEFINITION);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_func_definition_requires_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_func_definition_requires_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER,
            TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::FUNC_DEFINITION);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST FUNC DEFINITION ---
namespace {
    TestResult test_extract_func_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("FUNC_DEFINITION_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_func_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_func_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::FUNC_DEFINITION);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_func_definition_requires_single() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_func_definition_requires_single", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON,
            TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::FUNC_DEFINITION);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 9;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_func_definition_requires_multiple() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_func_definition_requires_multiple", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER,
            TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::FUNC_DEFINITION);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 12;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_func_definition_tests() {
    function_list func_definition_tests = {
        // Match Tests
        test_match_func_definition,
        test_match_func_definition_normal,
        test_match_func_definition_requires_single,
        test_match_func_definition_requires_multiple,
        // Contain Tests
        test_contain_func_definition,
        test_contain_func_definition_normal,
        test_contain_func_definition_requires_single,
        test_contain_func_definition_requires_multiple,
        // Extract Tests
        test_extract_func_definition,
        test_extract_func_definition_normal,
        test_extract_func_definition_requires_single,
        test_extract_func_definition_requires_multiple,
    };
    return func_definition_tests;
}
