#pragma once

#include "debug.hpp"
#include "matcher/matcher.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST PRIMARY ---
namespace {
    TestResult test_match_prim() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("PRIMARY TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("PRIMARY_MATCH:", true);
        return test_result;
    }

    TestResult test_match_prim_int() {
        TestResult test_result;
        ;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_prim_int", false);
        token_list tokens = create_token_vector({//
            TOK_I32});
        bool result = Matcher::tokens_match({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_prim_flint() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_prim_flint", false);
        token_list tokens = create_token_vector({//
            TOK_FLINT});
        bool result = Matcher::tokens_match({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_prim_str() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_prim_str", false);
        token_list tokens = create_token_vector({//
            TOK_STR});
        bool result = Matcher::tokens_match({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_prim_char() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_char_int", false);
        token_list tokens = create_token_vector({//
            TOK_CHAR});
        bool result = Matcher::tokens_match({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_prim_bool() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_prim_bool", false);
        token_list tokens = create_token_vector({//
            TOK_BOOL});
        bool result = Matcher::tokens_match({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST PRIMARY ---
namespace {
    TestResult test_contain_prim() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("PRIMARY_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_prim_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_prim_int", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_EOL, TOK_I32, TOK_DATA});
        bool result = Matcher::tokens_contain({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_prim_flint() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_prim_flint", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_EOL, TOK_FLINT, TOK_DATA});
        bool result = Matcher::tokens_contain({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_prim_str() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_prim_str", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_EOL, TOK_STR, TOK_DATA});
        bool result = Matcher::tokens_contain({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_prim_char() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_prim_char", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_EOL, TOK_CHAR, TOK_DATA});
        bool result = Matcher::tokens_contain({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_prim_bool() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_prim_bool", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_EOL, TOK_BOOL, TOK_DATA});
        bool result = Matcher::tokens_contain({tokens.begin(), tokens.end()}, Matcher::type_prim);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST PRIMARY ---
namespace {
    TestResult test_extract_prim() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("PRIMARY_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_prim_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_prim_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_I32, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges({tokens.begin(), tokens.end()}, Matcher::type_prim);
        bool result = !result_vec.empty() && result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_prim_flint() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_prim_flint", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_FLINT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges({tokens.begin(), tokens.end()}, Matcher::type_prim);
        bool result = !result_vec.empty() && result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_prim_str() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_prim_str", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_STR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges({tokens.begin(), tokens.end()}, Matcher::type_prim);
        bool result = !result_vec.empty() && result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_prim_char() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_prim_char", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_CHAR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges({tokens.begin(), tokens.end()}, Matcher::type_prim);
        bool result = !result_vec.empty() && result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_prim_bool() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_prim_bool", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_BOOL, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Matcher::get_match_ranges({tokens.begin(), tokens.end()}, Matcher::type_prim);
        bool result = !result_vec.empty() && result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_primary_tests() {
    function_list primary_tests = {
        // Match Tests Primary
        test_match_prim,
        test_match_prim_int,
        test_match_prim_flint,
        test_match_prim_str,
        test_match_prim_char,
        test_match_prim_bool,
        // Contain Tests Primary
        test_contain_prim,
        test_contain_prim_int,
        test_contain_prim_flint,
        test_contain_prim_str,
        test_contain_prim_char,
        test_contain_prim_bool,
        // Extract Tests Primary
        test_extract_prim,
        test_extract_prim_int,
        test_extract_prim_flint,
        test_extract_prim_str,
        test_extract_prim_char,
        test_extract_prim_bool,
    };
    return primary_tests;
}
