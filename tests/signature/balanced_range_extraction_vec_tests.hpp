#pragma once

#include "debug.hpp"
#include "matcher/matcher.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- TEST BALANCED RANGE EXTRACTION VEC ---
namespace {
    TestResult test_balanced_range_extraction_vec() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("BALANCED_RANGE_EXTRACTION_VEC:", true);
        return test_result;
    }

    TestResult test_balanced_range_extraction_vec_lr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_balanced_range_extraction_vec_lr", false);
        // x := func()
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::vector<uint2> ranges = Matcher::balanced_range_extraction_vec(                                 //
            {tokens.begin(), tokens.end()}, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
        );
        bool result = ranges.size() == 1 && ranges.at(0).first == 3 && ranges.at(0).second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_balanced_range_extraction_vec_llrlrlrr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_balanced_range_extraction_vec_llrlrlrr", false);
        // x := func((a * b) - func2() - func3());
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_MULT, TOK_IDENTIFIER,
            TOK_RIGHT_PAREN, TOK_MINUS, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_MINUS, TOK_IDENTIFIER, TOK_LEFT_PAREN,
            TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::vector<uint2> ranges = Matcher::balanced_range_extraction_vec(                                 //
            {tokens.begin(), tokens.end()}, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
        );
        bool result = ranges.size() == 1 && ranges.at(0).first == 3 && ranges.at(0).second == 18;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_balanced_range_extraction_vec_llrrlr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_balanced_range_extraction_vec_llrrlr", false);
        // x := (a * func(2)) ** (3 - 4 * 5);
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_MULT, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT_VALUE,
            TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_POW, TOK_LEFT_PAREN, TOK_INT_VALUE, TOK_MINUS, TOK_INT_VALUE, TOK_MULT, TOK_INT_VALUE,
            TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::vector<uint2> ranges = Matcher::balanced_range_extraction_vec(                                 //
            {tokens.begin(), tokens.end()}, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
        );
        bool result = ranges.size() == 2 && ranges.at(0).first == 2 && ranges.at(0).second == 10 && ranges.at(1).first == 11 &&
            ranges.at(1).second == 18;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_balanced_range_extraction_vec_tests() {
    function_list balanced_range_extraction_vec = {
        test_balanced_range_extraction_vec,
        test_balanced_range_extraction_vec_lr,
        test_balanced_range_extraction_vec_llrlrlrr,
        test_balanced_range_extraction_vec_llrrlr,
    };
    return balanced_range_extraction_vec;
}
