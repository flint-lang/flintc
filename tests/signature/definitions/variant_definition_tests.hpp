#pragma once

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST VARIANT DEFINITION ---
namespace {
    TestResult test_match_variant_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("VARIANT_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("VARIANT_DEFINITION_MATCH:", true);
        return test_result;
    }

    TestResult test_match_variant_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_variant_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON});
        bool result = Signature::tokens_match(tokens, Signature::variant_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST VARIANT DEFINITION ---
namespace {
    TestResult test_contain_variant_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("VARIANT_DEFINITION_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_variant_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_variant_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, Signature::variant_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST VARIANT DEFINITION ---
namespace {
    TestResult test_extract_variant_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("VARIANT_DEFINITION_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_variant_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_variant_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::variant_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_variant_definition_tests() {
    function_list variant_definition_tests = {
        // Match Tests
        test_match_variant_definition,
        test_match_variant_definition_normal,
        // Contain Tests
        test_contain_variant_definition,
        test_contain_variant_definition_normal,
        // Extract Tests
        test_extract_variant_definition,
        test_extract_variant_definition_normal,
    };
    return variant_definition_tests;
}
