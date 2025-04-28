#pragma once

#include "debug.hpp"
#include "matcher/matcher.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST FUNCTION DEFINITION ---
namespace {
    TestResult test_match_function_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNCTION_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNCTION_DEFINITION_MATCH:", true);
        return test_result;
    }

    TestResult test_match_function_definition_const() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_const", false);
        token_list tokens = create_token_vector({//
            TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_aligned() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_aligned", false);
        token_list tokens = create_token_vector({//
            TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_aligned_const() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_aligned_const", false);
        token_list tokens = create_token_vector({//
            TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_0arg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_0arg_0return", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_1arg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_1arg_0return", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_0arg_1return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_0arg_1return", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_I32, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_1arg_1return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_1arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_I32, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_narg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_narg_0return", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN,
            TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_0arg_nreturn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_function_definition_0arg_nreturn", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT,
            TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_function_definition_narg_nreturn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_function_definition_narg_nreturn", false);
        token_list tokens = create_token_vector({//
            TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN,
            TOK_ARROW, TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON});
        bool result = Matcher::tokens_match(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST FUNCTION DEFINITION ---
namespace {
    TestResult test_contain_function_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("FUNCTION_DEFINITION_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_function_definition_const() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_const", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_aligned() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_aligned", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_aligned_const() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_aligned_const", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_0arg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_0arg_0return", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_1arg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_1arg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_0arg_1return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_0arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_I32, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_1arg_1return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_1arg_1return", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_I32, TOK_COLON,
            TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_narg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_narg_0return", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER,
            TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_0arg_nreturn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_function_definition_0arg_nreturn", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT,
            TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_function_definition_narg_nreturn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_function_definition_narg_nreturn", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER,
                TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        bool result = Matcher::tokens_contain(tokens, Matcher::function_definition);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST FUNCTION DEFINITION ---
namespace {
    TestResult test_extract_function_definition() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("FUNCTION_DEFINITION_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_function_definition_const() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_const", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_aligned() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_aligned", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_aligned_const() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_aligned_const", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_0arg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_0arg_0return", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_1arg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_1arg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_0arg_1return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_0arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_I32, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_1arg_1return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_1arg_1return", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_I32, TOK_COLON,
            TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 10;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_narg_0return() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_narg_0return", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER,
            TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 11;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_0arg_nreturn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_function_definition_0arg_nreturn", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT,
            TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 12;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_function_definition_narg_nreturn() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_function_definition_narg_nreturn", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_I32, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER,
                TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_I32, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Matcher::get_match_ranges(tokens, Matcher::function_definition);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 17;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_function_definition_tests() {
    function_list function_definition_tests = {
        // Match Tests
        test_match_function_definition,
        test_match_function_definition_const,
        test_match_function_definition_aligned,
        test_match_function_definition_aligned_const,
        test_match_function_definition_0arg_0return,
        test_match_function_definition_1arg_0return,
        test_match_function_definition_0arg_1return,
        test_match_function_definition_1arg_1return,
        test_match_function_definition_narg_0return,
        test_match_function_definition_0arg_nreturn,
        test_match_function_definition_narg_nreturn,
        // Contain Tests
        test_contain_function_definition,
        test_contain_function_definition_const,
        test_contain_function_definition_aligned,
        test_contain_function_definition_aligned_const,
        test_contain_function_definition_0arg_0return,
        test_contain_function_definition_1arg_0return,
        test_contain_function_definition_0arg_1return,
        test_contain_function_definition_1arg_1return,
        test_contain_function_definition_narg_0return,
        test_contain_function_definition_0arg_nreturn,
        test_contain_function_definition_narg_nreturn,
        // Extract Tests
        test_extract_function_definition,
        test_extract_function_definition_const,
        test_extract_function_definition_aligned,
        test_extract_function_definition_aligned_const,
        test_extract_function_definition_0arg_0return,
        test_extract_function_definition_1arg_0return,
        test_extract_function_definition_0arg_1return,
        test_extract_function_definition_1arg_1return,
        test_extract_function_definition_narg_0return,
        test_extract_function_definition_0arg_nreturn,
        test_extract_function_definition_narg_nreturn,
    };
    return function_definition_tests;
}
