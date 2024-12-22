#ifndef __TYPE_TESTS_HPP__
#define __TYPE_TESTS_HPP__

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST TYPE ---
namespace {
    TestResult test_match_type() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("TYPE TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("TYPE_MATCH:", true);
        return test_result;
    }

    TestResult test_match_type_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_type_int", false);
        token_list tokens = create_token_vector({//
            TOK_INT});
        bool result = Signature::tokens_match(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_type_flint() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_type_flint", false);
        token_list tokens = create_token_vector({//
            TOK_FLINT});
        bool result = Signature::tokens_match(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_type_str() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_type_str", false);
        token_list tokens = create_token_vector({//
            TOK_STR});
        bool result = Signature::tokens_match(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_type_char() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_type_char", false);
        token_list tokens = create_token_vector({//
            TOK_CHAR});
        bool result = Signature::tokens_match(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_type_bool() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_type_bool", false);
        token_list tokens = create_token_vector({//
            TOK_BOOL});
        bool result = Signature::tokens_match(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_match_type_identifier() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_match_type_identifier", false);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER});
        bool result = Signature::tokens_match(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST TYPE ---
namespace {
    TestResult test_contain_type() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("TYPE_CONTAIN:", true);
        return test_result;
    }

    TestResult test_contain_type_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_type_int", false);
        token_list tokens = create_token_vector({//
            TOK_COLON, TOK_INT, TOK_DATA});
        bool result = Signature::tokens_contain(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_type_flint() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_type_flint", false);
        token_list tokens = create_token_vector({//
            TOK_COLON, TOK_FLINT, TOK_DATA});
        bool result = Signature::tokens_contain(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_type_str() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_type_str", false);
        token_list tokens = create_token_vector({//
            TOK_COLON, TOK_STR, TOK_DATA});
        bool result = Signature::tokens_contain(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_type_char() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_type_char", false);
        token_list tokens = create_token_vector({//
            TOK_COLON, TOK_CHAR, TOK_DATA});
        bool result = Signature::tokens_contain(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_type_bool() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_type_bool", false);
        token_list tokens = create_token_vector({//
            TOK_COLON, TOK_BOOL, TOK_DATA});
        bool result = Signature::tokens_contain(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_contain_type_identifier() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_contain_type_identifier", false);
        token_list tokens = create_token_vector({//
            TOK_COLON, TOK_IDENTIFIER, TOK_DATA});
        bool result = Signature::tokens_contain(tokens, Signature::type);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// -- EXTRACT TEST TYPE ---
namespace {
    TestResult test_extract_type() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("TYPE_EXTRACT:", true);
        return test_result;
    }

    TestResult test_extract_type_int() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_type_int", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() && result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_type_flint() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_type_flint", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_FLINT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() && result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_type_str() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_type_str", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_STR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() && result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_type_char() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_type_char", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_CHAR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() && result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_type_bool() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_extract_type_bool", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_BOOL, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() && result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_extract_type_identifier() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_type_identifier", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty();
        result = result && result_vec.at(0).first == 1 && result_vec.at(0).second == 2;
        result = result && result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        result = result && result_vec.at(2).first == 4 && result_vec.at(2).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_type_tests() {
    function_list type_tests = {
        // Match Tests Type
        test_match_type,
        test_match_type_int,
        test_match_type_flint,
        test_match_type_str,
        test_match_type_char,
        test_match_type_bool,
        test_match_type_identifier,
        // Contain Tests Type
        test_contain_type,
        test_contain_type_int,
        test_contain_type_flint,
        test_contain_type_str,
        test_contain_type_char,
        test_contain_type_bool,
        test_contain_type_identifier,
        // Extract Tests Type
        test_extract_type,
        test_extract_type_int,
        test_extract_type_flint,
        test_extract_type_str,
        test_extract_type_char,
        test_extract_type_bool,
        test_extract_type_identifier,
    };
    return type_tests
}

#endif
