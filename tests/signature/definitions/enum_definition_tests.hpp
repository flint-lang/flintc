#pragma once

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST ENUM DEFINITION ---
namespace {
    TestResult test_match_enum_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("ENUM_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("ENUM_DEFINITION_MATCH:", true);
        return test_result;
    }

    TestResult test_match_enum_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_enum_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_ENUM, TOK_IDENTIFIER, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::ENUM_DEFINITION);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST ENUM DEFINITION ---
namespace {
    TestResult test_contain_enum_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("ENUM_DEFINITION_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_enum_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_enum_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ENUM, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::ENUM_DEFINITION);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST ENUM DEFINITION ---
namespace {
    TestResult test_extract_enum_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("ENUM_DEFINITION_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_enum_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_enum_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ENUM, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::ENUM_DEFINITION);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_enum_definition_tests() {
    function_list enum_definition_tests = {
        // Match Tests
        test_match_enum_definition,
        test_match_enum_definition_normal,
        // Contain Tests
        test_contain_enum_definition,
        test_contain_enum_definition_normal,
        // Extract Tests
        test_extract_enum_definition,
        test_extract_enum_definition_normal,
    };
    return enum_definition_tests;
}
