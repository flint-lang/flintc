#ifndef __DATA_DEFINITION_TESTS_HPP__
#define __DATA_DEFINITION_TESTS_HPP__

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST DATA DEFINITION ---
namespace {
    TestResult test_match_data_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("DATA_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("DATA_DEFINITION_MATCH:", true);
        return test_result;
    }

    TestResult test_match_data_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_data_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_DATA, TOK_IDENTIFIER, TOK_COLON});
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_data_definition_shared() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_data_definition_shared", false);
        token_list tokens = create_token_vector({//
            TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON});
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_data_definition_immutable() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_data_definition_immutable", false);
        token_list tokens = create_token_vector({//
            TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON});
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_data_definition_aligned() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_data_definition_aligned", false);
        token_list tokens = create_token_vector({//
            TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON});
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST DATA DEFINITION ---
namespace {
    TestResult test_contain_data_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("DATA_DEFINITION_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_data_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_data_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_data_definition_shared() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_data_definition_shared", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_data_definition_immutable() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_data_definition_immutable", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_data_definition_aligned() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_data_definition_aligned", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST DATA DEFINITION ---
namespace {
    TestResult test_extract_data_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("DATA_DEFINITION_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_data_definition_normal() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_data_definition_normal", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::data_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_data_definition_shared() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_data_definition_shared", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::data_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_data_definition_immutable() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_data_definition_immutable", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::data_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_data_definition_aligned() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_data_definition_aligned", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::data_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_data_definition_tests() {
    function_list data_definition_tests = {
        // Match Tests
        test_match_data_definition,
        test_match_data_definition_normal,
        test_match_data_definition_shared,
        test_match_data_definition_immutable,
        test_match_data_definition_aligned,
        // Contain Tests
        test_contain_data_definition,
        test_contain_data_definition_normal,
        test_contain_data_definition_shared,
        test_contain_data_definition_immutable,
        test_contain_data_definition_aligned,
        // Extract Tests
        test_extract_data_definition,
        test_extract_data_definition_normal,
        test_extract_data_definition_shared,
        test_extract_data_definition_immutable,
        test_extract_data_definition_aligned,
    };
    return data_definition_tests;
}

#endif
