#pragma once

#include "debug.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

namespace {
    TestResult test_balanced_range_extraction() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("BALANCED_RANGE_EXTRACTION:", true);
        return test_result;
    }

    TestResult test_balanced_range_extraction_lr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_balanced_range_extraction_lr", false);
        // x := func()
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::optional<uint2> range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 5;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_balanced_range_extraction_llrr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_balanced_range_extraction_llrr", false);
        // x := func( func2() )
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN,
            TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::optional<uint2> range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 8;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_balanced_range_extraction_llrlrr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_balanced_range_extraction_llrlrr", false);
        // x := func( (a + b) * (b - a) )
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_PLUS, TOK_IDENTIFIER,
            TOK_RIGHT_PAREN, TOK_MULT, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_MINUS, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::optional<uint2> range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 15;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_balanced_range_extraction_lllrrr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_balanced_range_extraction_lllrrr", false);
        // x := func( func2( func3() ) );
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN,
            TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::optional<uint2> range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 11;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }

    TestResult test_balanced_range_extraction_llrlrlrr() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_balanced_range_extraction_llrlrlrr", false);
        // x := func((a * b) - func2() - func3());
        token_list tokens = create_token_vector({//
            TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_MULT, TOK_IDENTIFIER,
            TOK_RIGHT_PAREN, TOK_MINUS, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_MINUS, TOK_IDENTIFIER, TOK_LEFT_PAREN,
            TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON});
        std::optional<uint2> range = Signature::balanced_range_extraction(tokens, LEFT_PAREN_STR, RIGHT_PAREN_STR);
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 18;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_balanced_range_extraction_tests() {
    function_list balanced_range_extraction = {
        test_balanced_range_extraction,
        test_balanced_range_extraction_lr,
        test_balanced_range_extraction_llrr,
        test_balanced_range_extraction_llrlrr,
        test_balanced_range_extraction_lllrrr,
        test_balanced_range_extraction_llrlrlrr,
    };
    return balanced_range_extraction;
}
