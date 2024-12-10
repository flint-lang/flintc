#ifndef __TEST_SIGNATURE_HPP__
#define __TEST_SIGNATURE_HPP__

#include "signature.hpp"
#include "../test_utils.hpp"
#include "../debug.hpp"

#include <string>
#include <vector>

// --- THE SIGNATURE TESTS ---

// --- TEST SIGNATURE METHODS ---
// --- TEST BALANCED RANGE EXTRACTION ---
namespace {
    int test_balanced_range_extraction() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("BALANCED_RANGE_EXTRACTION:", true);
        return 0;
    }

    int test_balanced_range_extraction_lr() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_balanced_range_extraction_lr", false);
        // x := func()
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        std::optional<std::pair<unsigned int, unsigned int>> range = Signature::balanced_range_extraction(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_balanced_range_extraction_llrr() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_balanced_range_extraction_llrr", false);
        // x := func( func2() )
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        std::optional<std::pair<unsigned int, unsigned int>> range = Signature::balanced_range_extraction(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 8;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_balanced_range_extraction_llrlrr() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_balanced_range_extraction_llrlrr", false);
        // x := func( (a + b) * (b - a) )
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_PLUS, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_MULT, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_MINUS, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        std::optional<std::pair<unsigned int, unsigned int>> range = Signature::balanced_range_extraction(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 15;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_balanced_range_extraction_lllrrr() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_balanced_range_extraction_lllrrr", false);
        // x := func( func2( func3() ) );
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        std::optional<std::pair<unsigned int, unsigned int>> range = Signature::balanced_range_extraction(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 11;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_balanced_range_extraction_llrlrlrr() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_balanced_range_extraction_llrlrlrr", false);
        // x := func((a * b) - func2() - func3());
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_MULT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_MINUS, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_MINUS, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        std::optional<std::pair<unsigned int, unsigned int>> range = Signature::balanced_range_extraction(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
        bool result = range.has_value() && range.value().first == 3 && range.value().second == 18;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}


// --- PRIMARY TESTS ---
// --- MATCH TEST PRIMARY ---
namespace {
    int test_match_prim() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("PRIMARY TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("PRIMARY_MATCH:", true);
        return 0;
    }

    int test_match_prim_int() {;
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_prim_int", false);
        token_list tokens = create_token_vector(
            {TOK_INT}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_prim_flint() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_prim_flint", false);
        token_list tokens = create_token_vector(
            {TOK_FLINT}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_prim_str() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_prim_str", false);
        token_list tokens = create_token_vector(
            {TOK_STR}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_prim_char() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
            TestUtils::print_test_name("test_match_char_int", false);
        token_list tokens = create_token_vector(
            {TOK_CHAR}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_prim_bool() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_prim_bool", false);
        token_list tokens = create_token_vector(
            {TOK_BOOL}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST PRIMARY ---
namespace {
    int test_contain_prim() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("PRIMARY_CONTAIN:", true);
        return 0;
    }

    int test_contain_prim_int() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_prim_int", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_INT, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_prim_flint() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_prim_flint", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_FLINT, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_prim_str() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_prim_str", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_STR, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_prim_char() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_prim_char", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_CHAR, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_prim_bool() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_prim_bool", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_BOOL, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST PRIMARY ---
namespace {
    int test_extract_prim() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("PRIMARY_EXTRACT:", true);
        return 0;
    }

    int test_extract_prim_int() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_prim_int", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_prim_flint() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_prim_flint", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_FLINT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_prim_str() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_prim_str", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_STR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_prim_char() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_prim_char", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_CHAR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_prim_bool() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_prim_bool", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_BOOL, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- TYPE TESTS ---
// --- MATCH TEST TYPE ---
namespace {
    int test_match_type() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("TYPE TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("TYPE_MATCH:", true);
        return 0;
    }

    int test_match_type_int() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_type_int", false);
        token_list tokens = create_token_vector(
            {TOK_INT}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_flint() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_type_flint", false);
        token_list tokens = create_token_vector(
            {TOK_FLINT}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_str() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_type_str", false);
        token_list tokens = create_token_vector(
            {TOK_STR}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_char() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_type_char", false);
        token_list tokens = create_token_vector(
            {TOK_CHAR}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_bool() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_type_bool", false);
        token_list tokens = create_token_vector(
            {TOK_BOOL}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_identifier() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_type_identifier", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST TYPE ---
namespace {
    int test_contain_type() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("TYPE_CONTAIN:", true);
        return 0;
    }

    int test_contain_type_int() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_type_int", false);
        token_list tokens = create_token_vector(
            {TOK_COLON, TOK_INT, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_flint() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_type_flint", false);
        token_list tokens = create_token_vector(
            {TOK_COLON, TOK_FLINT, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_str() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_type_str", false);
        token_list tokens = create_token_vector(
            {TOK_COLON, TOK_STR, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_char() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_type_char", false);
        token_list tokens = create_token_vector(
            {TOK_COLON, TOK_CHAR, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_bool() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_type_bool", false);
        token_list tokens = create_token_vector(
            {TOK_COLON, TOK_BOOL, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_identifier() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_type_identifier", false);
        token_list tokens = create_token_vector(
            {TOK_COLON, TOK_IDENTIFIER, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// -- EXTRACT TEST TYPE ---
namespace {
    int test_extract_type() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("TYPE_EXTRACT:", true);
        return 0;
    }

    int test_extract_type_int() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_type_int", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_flint() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_type_flint", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_FLINT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_str() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_type_str", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_STR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_char() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_type_char", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_CHAR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_bool() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_type_bool", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_BOOL, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_identifier() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_type_identifier", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::type);
        bool result = !result_vec.empty();
        result = result && result_vec.at(0).first == 1 && result_vec.at(0).second == 2;
        result = result && result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        result = result && result_vec.at(2).first == 4 && result_vec.at(2).second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- REFERENCE TESTS ---
// --- MATCH TEST REFERENCE ---
namespace {
    int test_match_reference() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("REFERENCE TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("REFERENCE_MATCH:", true);
        return 0;
    }

    int test_match_reference_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_reference_single", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::reference);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_reference_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_reference_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::reference);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST REFERENCE ---
namespace {
    int test_contain_reference() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("REFERENCE_CONTAIN:", true);
        return 0;
    }

    int test_contain_reference_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_reference_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::reference);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_reference_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_reference_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::reference);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST REFERENCE ---
namespace {
    int test_extract_reference() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("REFERENCE_EXTRACT:", true);
        return 0;
    }

    int test_extract_reference_single() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_reference_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::reference);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 4 && result_vec.at(0).second == tokens.size() - 1;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_reference_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_reference_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::reference);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 4 && result_vec.at(0).second == tokens.size() - 1;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- ARGS TESTS ---
// --- MATCH TEST ARGS ---
namespace {
    int test_match_args() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("ARGS TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("ARGS_MATCH:", true);
        return 0;
    }

    int test_match_args_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_args_single", false);
        token_list tokens = create_token_vector(
            {TOK_INT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::args);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_args_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_args_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::args);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST ARGS ---
namespace {
    int test_contain_args() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("ARGS_CONTAIN:", true);
        return 0;
    }

    int test_contain_args_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_args_single", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::args);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_args_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_args_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::args);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST ARGS ---
namespace {
    int test_extract_args() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("ARGS_EXTRACT:", true);
        return 0;
    }

    int test_extract_args_single() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_args_single", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::args);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 3 && result_vec.at(0).second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_args_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_args_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::args);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 3 && result_vec.at(0).second == 8;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- GROUP TESTS ---
// --- MATCH TEST GROUP ---
namespace {
    int test_match_group() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("GROUP TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("GROUP_MATCH:", true);
        return 0;
    }

    int test_match_group_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_group_single", false);
        token_list tokens = create_token_vector(
            {TOK_LEFT_PAREN, TOK_INT, TOK_RIGHT_PAREN}
        );
        bool result = Signature::tokens_match(tokens, Signature::group);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_group_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_group_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN}
        );
        bool result = Signature::tokens_match(tokens, Signature::group);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST GROUP ---
namespace {
    int test_contain_group() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("GROUP_CONTAIN:", true);
        return 0;
    }

    int test_contain_group_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_group_single", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::group);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_group_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_group_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::group);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST GROUP ---
namespace {
    int test_extract_group() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("GROUP_EXTRACT:", true);
        return 0;
    }

    int test_extract_group_single() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_group_single", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::group);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 7 && result_vec.at(0).second == tokens.size() - 1;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_group_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_group_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::group);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 10 && result_vec.at(0).second == 15;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- USE STATEMENT TESTS ---
// --- MATCH TEST USE STATEMENT ---
namespace {
    int test_match_use_statement() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("USE_STATEMENT TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("USE_STATEMENT_MATCH:", true);
        return 0;
    }

    int test_match_use_statement_string() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_use_statement_string", false);
        token_list tokens = create_token_vector(
            {TOK_USE, TOK_STR_VALUE}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_package_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_use_statement_package_single", false);
        token_list tokens = create_token_vector(
            {TOK_USE, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_package_dual() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_use_statement_package_dual", false);
        token_list tokens = create_token_vector(
            {TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_package_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_use_statement_package_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_flint_package_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_use_statement_flint_package_single", false);
        token_list tokens = create_token_vector(
            {TOK_USE, TOK_FLINT}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_flint_package_dual() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_use_statement_flint_package_dual", false);
        token_list tokens = create_token_vector(
            {TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_flint_package_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_use_statement_flint_package_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST USE STATEMENT ---
namespace {
    int test_contain_use_statement() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("USE_STATEMENT_CONTAIN:", true);
        return 0;
    }

    int test_contain_use_statement_string() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_use_statement_string", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_STR_VALUE, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_package_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_use_statement_package_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_package_dual() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_use_statement_package_dual", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_package_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_use_statement_package_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_flint_package_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_use_statement_flint_package_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_FLINT, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_flint_package_dual() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_use_statement_flint_package_dual", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_flint_package_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_use_statement_flint_package_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST USE STATEMENT ---
namespace {
    int test_extract_use_statement() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("USE_STATEMENT_EXTRACT:", true);
        return 0;
    }

    int test_extract_use_statement_string() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_use_statement_string", false);
        token_list tokens = create_token_vector(
           {TOK_INDENT, TOK_USE, TOK_STR_VALUE, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_package_single() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_use_statement_package_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_package_dual() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_use_statement_package_dual", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_package_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_use_statement_package_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_flint_package_single() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_use_statement_flint_package_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_FLINT, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_flint_package_dual() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_use_statement_flint_package_dual", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_flint_package_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_use_statement_flint_package_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_FLINT, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- FUNCTION DEFINITION TESTS ---
// --- MATCH TEST FUNCTION DEFINITION ---
namespace {
    int test_match_function_definition() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("FUNCTION_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("FUNCTION_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_function_definition_const() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_const", false);
        token_list tokens = create_token_vector(
            {TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_aligned() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_aligned", false);
        token_list tokens = create_token_vector(
            {TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_aligned_const() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_aligned_const", false);
        token_list tokens = create_token_vector(
            {TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_0arg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_0arg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_1arg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_1arg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_0arg_1return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_0arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_1arg_1return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_1arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_narg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_narg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_0arg_nreturn() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_definition_0arg_nreturn", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_narg_nreturn() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_function_definition_narg_nreturn", false);
        token_list tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST FUNCTION DEFINITION ---
namespace {
    int test_contain_function_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("FUNCTION_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_function_definition_const() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_const", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_aligned() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_aligned", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_aligned_const() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_aligned_const", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_0arg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_0arg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_1arg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_1arg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_0arg_1return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_0arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_1arg_1return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_1arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_narg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_narg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_0arg_nreturn() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_definition_0arg_nreturn", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_narg_nreturn() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_function_definition_narg_nreturn", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST FUNCTION DEFINITION ---
namespace {
    int test_extract_function_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("FUNCTION_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_function_definition_const() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_const", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_aligned() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_aligned", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_aligned_const() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_aligned_const", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_0arg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_0arg_0return", false);
        token_list tokens = create_token_vector(
           {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_1arg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_1arg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_0arg_1return() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_0arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_1arg_1return() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_1arg_1return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 10;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_narg_0return() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_narg_0return", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 11;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_0arg_nreturn() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_definition_0arg_nreturn", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 12;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_narg_nreturn() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_function_definition_narg_nreturn", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 17;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- DATA DEFINITION TESTS ---
// --- MATCH TEST DATA DEFINITION ---
namespace {
    int test_match_data_definition() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("DATA_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("DATA_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_data_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_data_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_DATA, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_data_definition_shared() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_data_definition_shared", false);
        token_list tokens = create_token_vector(
            {TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_data_definition_immutable() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_data_definition_immutable", false);
        token_list tokens = create_token_vector(
            {TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_data_definition_aligned() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_data_definition_aligned", false);
        token_list tokens = create_token_vector(
            {TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST DATA DEFINITION ---
namespace {
    int test_contain_data_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("DATA_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_data_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_data_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_data_definition_shared() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_data_definition_shared", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_data_definition_immutable() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_data_definition_immutable", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_data_definition_aligned() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_data_definition_aligned", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST DATA DEFINITION ---
namespace {
    int test_extract_data_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("DATA_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_data_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_data_definition_normal", false);
        token_list tokens = create_token_vector(
           {TOK_INDENT, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::data_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_data_definition_shared() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_data_definition_shared", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::data_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_data_definition_immutable() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_data_definition_immutable", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::data_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_data_definition_aligned() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_data_definition_aligned", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::data_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- FUNC DEFINITION TESTS ---
// --- MATCH TEST FUNC DEFINITION ---
namespace {
    int test_match_func_definition() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("FUNC_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("FUNC_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_func_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_func_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_FUNC, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::func_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_func_definition_requires_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_func_definition_requires_single", false);
        token_list tokens = create_token_vector(
            {TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::func_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_func_definition_requires_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_func_definition_requires_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::func_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST FUNC DEFINITION ---
namespace {
    int test_contain_func_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("FUNC_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_func_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_func_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::func_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_func_definition_requires_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_func_definition_requires_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::func_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_func_definition_requires_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_func_definition_requires_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::func_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST FUNC DEFINITION ---
namespace {
    int test_extract_func_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("FUNC_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_func_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_func_definition_normal", false);
        token_list tokens = create_token_vector(
           {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::func_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_func_definition_requires_single() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_func_definition_requires_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::func_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 9;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_func_definition_requires_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_func_definition_requires_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::func_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 12;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- ERROR DEFINITION TESTS ---
// --- MATCH TEST ERROR DEFINITION ---
namespace {
    int test_match_error_definition() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("ERROR_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("ERROR_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_error_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_error_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_ERROR, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::error_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_error_definition_extending() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_error_definition_extending", false);
        token_list tokens = create_token_vector(
            {TOK_ERROR, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::error_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST ERROR DEFINITION ---
namespace {
    int test_contain_error_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("ERROR_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_error_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_error_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ERROR, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::error_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_error_definition_extending() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_error_definition_extending", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ERROR, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::error_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST ERROR DEFINITION ---
namespace {
    int test_extract_error_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("ERROR_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_error_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_error_definition_normal", false);
        token_list tokens = create_token_vector(
           {TOK_INDENT, TOK_ERROR, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::error_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_error_definition_extending() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_error_definition_extending", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ERROR, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::error_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- ENUM DEFINITION TESTS ---
// --- MATCH TEST ENUM DEFINITION ---
namespace {
    int test_match_enum_definition() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("ENUM_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("ENUM_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_enum_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_enum_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_ENUM, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::enum_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST ENUM DEFINITION ---
namespace {
    int test_contain_enum_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("ENUM_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_enum_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_enum_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ENUM, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::enum_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST ENUM DEFINITION ---
namespace {
    int test_extract_enum_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("ENUM_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_enum_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_enum_definition_normal", false);
        token_list tokens = create_token_vector(
           {TOK_INDENT, TOK_ENUM, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::enum_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- VARIANT DEFINITION TESTS ---
// --- MATCH TEST VARIANT DEFINITION ---
namespace {
    int test_match_variant_definition() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("VARIANT_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("VARIANT_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_variant_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_variant_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::variant_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST VARIANT DEFINITION ---
namespace {
    int test_contain_variant_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("VARIANT_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_variant_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_variant_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::variant_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST VARIANT DEFINITION ---
namespace {
    int test_extract_variant_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("VARIANT_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_variant_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_variant_definition_normal", false);
        token_list tokens = create_token_vector(
        {TOK_INDENT, TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::variant_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- ENTITY DEFINITION TESTS ---
// --- MATCH TEST ENTITY DEFINITION ---
namespace {
    int test_match_entity_definition() {
        Debug::print_tree_row({Debug::BRANCH}, true);
        TestUtils::print_test_name("ENTITY_DEFINITION TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("ENTITY_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_entity_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_entity_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_ENTITY, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::entity_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_entity_definition_extends_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_entity_definition_extends_single", false);
        token_list tokens = create_token_vector(
            {TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::entity_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_entity_definition_extends_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_entity_definition_extends_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::entity_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST ENTITY DEFINITION ---
namespace {
    int test_contain_entity_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("ENTITY_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_entity_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_entity_definition_normal", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::entity_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_entity_definition_extends_single() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_entity_definition_extends_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::entity_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_entity_definition_extends_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_entity_definition_extends_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::entity_definition);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST ENTITY DEFINITION ---
namespace {
    int test_extract_entity_definition() {
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("ENTITY_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_entity_definition_normal() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_entity_definition_normal", false);
        token_list tokens = create_token_vector(
           {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::entity_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_entity_definition_extends_single() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_entity_definition_extends_single", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::entity_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 9;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_entity_definition_extends_multiple() {
        Debug::print_tree_row({Debug::VERT, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_entity_definition_extends_multiple", false);
        token_list tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::entity_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 12;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}


// --- FUNCTION CALL EXPRESSION TESTS ---
// --- MATCH TEST FUNCTION CALL EXPRESSION ---
namespace {
    int test_match_function_call() {
        Debug::print_tree_row({Debug::SINGLE}, true);
        TestUtils::print_test_name("FUNCTION_CALL TESTS:", true);
        Debug::print_tree_row({Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("FUNCTION_CALL_MATCH:", true);
        return 0;
    }

    int test_match_function_call_0arg() {
        Debug::print_tree_row({Debug::NONE, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_call_0arg", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_call);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_call_1arg_identifier() {
        Debug::print_tree_row({Debug::NONE, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_match_function_call_1arg_identifier", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_call);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_call_1arg_function_0arg() {
        Debug::print_tree_row({Debug::NONE, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_match_function_call_1arg_function_0arg", false);
        token_list tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_call);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST FUNCTION CALL EXPRESSION ---
namespace {
    int test_contain_function_call() {
        Debug::print_tree_row({Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("FUNCTION_CALL_CONTAIN:", true);
        return 0;
    }

    int test_contain_function_call_0arg() {
        Debug::print_tree_row({Debug::NONE, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_call_0arg", false);
        token_list tokens = create_token_vector(
            {TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_call);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_call_1arg_identifier() {
        Debug::print_tree_row({Debug::NONE, Debug::VERT, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_contain_function_call_1arg_identifier", false);
        token_list tokens = create_token_vector(
            {TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_call);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_call_1arg_function_0arg() {
        Debug::print_tree_row({Debug::NONE, Debug::VERT, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_contain_function_call_1arg_function_0arg", false);
        token_list tokens = create_token_vector(
            {TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_call);
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST FUNCTION CALL EXPRESSION ---
namespace {
    int test_extract_function_call() {
        Debug::print_tree_row({Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("FUNCTION_CALL_EXTRACT:", true);
        return 0;
    }

    int test_extract_function_call_0arg() {
        Debug::print_tree_row({Debug::NONE, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_call_0arg", false);
        token_list tokens = create_token_vector(
           {TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_call);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_call_1arg_identifier() {
        Debug::print_tree_row({Debug::NONE, Debug::NONE, Debug::BRANCH}, true);
        TestUtils::print_test_name("test_extract_function_call_1arg_identifier", false);
        token_list tokens = create_token_vector(
            {TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_call);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_call_1arg_function_0arg() {
        Debug::print_tree_row({Debug::NONE, Debug::NONE, Debug::SINGLE}, true);
        TestUtils::print_test_name("test_extract_function_call_1arg_function_0arg", false);
        token_list tokens = create_token_vector(
            {TOK_EQUAL, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_RIGHT_PAREN, TOK_SEMICOLON}
        );
        std::vector<std::pair<unsigned int, unsigned int>> result_vec = Signature::get_match_ranges(tokens, Signature::function_call);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        TestUtils::ok_or_not(result);
        return result ? 0 : 1;
    }
}

int test_signature() {
    TestUtils::print_test_name("SIGNATURE_TESTS:", true);

    // --- SIGNATURE METHODS ---
    function_list balanced_range_extraction = {
        test_balanced_range_extraction,
        test_balanced_range_extraction_lr,
        test_balanced_range_extraction_llrr,
        test_balanced_range_extraction_llrlrr,
        test_balanced_range_extraction_lllrrr,
        test_balanced_range_extraction_llrlrlrr,
    };
    // --- BASIC SIGNATURES ---
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
    function_list reference_tests = {
        // Match Tests Reference
        test_match_reference,
        test_match_reference_single,
        test_match_reference_multiple,
        // Contain Tests Reference
        test_contain_reference,
        test_contain_reference_single,
        test_contain_reference_multiple,
        // Extract Tests Reference
        test_extract_reference,
        test_extract_reference_single,
        test_extract_reference_multiple,
    };
    function_list args_tests = {
        // Match Tests
        test_match_args,
        test_match_args_single,
        test_match_args_multiple,
        // Contain Tests
        test_contain_args,
        test_contain_args_single,
        test_contain_args_multiple,
        // Extract Tests
        test_extract_args,
        test_extract_args_single,
        test_extract_args_multiple,
    };
    function_list group_tests = {
        // Match Tests
        test_match_group,
        test_match_group_single,
        test_match_group_multiple,
        // Contain Tests
        test_contain_group,
        test_contain_group_single,
        test_contain_group_multiple,
        // Extract Tests
        test_extract_group,
        test_extract_group_single,
        test_extract_group_multiple,
    };
    // --- DEFINITIONS ---
    function_list use_statement_tests = {
        // Match Tests
        test_match_use_statement,
        test_match_use_statement_string,
        test_match_use_statement_package_single,
        test_match_use_statement_package_dual,
        test_match_use_statement_package_multiple,
        test_match_use_statement_flint_package_single,
        test_match_use_statement_flint_package_dual,
        test_match_use_statement_flint_package_multiple,
        // Contain Tests
        test_contain_use_statement,
        test_contain_use_statement_string,
        test_contain_use_statement_package_single,
        test_contain_use_statement_package_dual,
        test_contain_use_statement_package_multiple,
        test_contain_use_statement_flint_package_single,
        test_contain_use_statement_flint_package_dual,
        test_contain_use_statement_flint_package_multiple,
        // Extract Tests
        test_extract_use_statement,
        test_extract_use_statement_string,
        test_extract_use_statement_package_single,
        test_extract_use_statement_package_dual,
        test_extract_use_statement_package_multiple,
        test_extract_use_statement_flint_package_single,
        test_extract_use_statement_flint_package_dual,
        test_extract_use_statement_flint_package_multiple,
    };
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
    function_list entity_definition_tests = {
        // Match Tests
        test_match_entity_definition,
        test_match_entity_definition_normal,
        test_match_entity_definition_extends_single,
        test_match_entity_definition_extends_multiple,
        // Contain Tests
        test_contain_entity_definition,
        test_contain_entity_definition_normal,
        test_contain_entity_definition_extends_single,
        test_contain_entity_definition_extends_multiple,
        // Extract Tests
        test_extract_entity_definition,
        test_extract_entity_definition_normal,
        test_extract_entity_definition_extends_single,
        test_extract_entity_definition_extends_multiple,
    };
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
    // --- EXPRESSIONS ---
    function_list function_call_tests = {
        // Match Tests
        test_match_function_call,
        test_match_function_call_0arg,
        test_match_function_call_1arg_identifier,
        test_match_function_call_1arg_function_0arg,
        // Contain Tests
        test_contain_function_call,
        test_contain_function_call_0arg,
        test_contain_function_call_1arg_identifier,
        test_contain_function_call_1arg_function_0arg,
        // Extract Tests
        test_extract_function_call,
        test_extract_function_call_0arg,
        test_extract_function_call_1arg_identifier,
        test_extract_function_call_1arg_function_0arg,
    };

    const std::vector<function_list> tests = {
        // --- SIGNATURE METHODS ---
        balanced_range_extraction,
        // --- BASIC SIGNATURES ---
        primary_tests,
        type_tests,
        reference_tests,
        args_tests,
        group_tests,
        // --- DEFINITIONS ---
        use_statement_tests,
        function_definition_tests,
        data_definition_tests,
        func_definition_tests,
        entity_definition_tests,
        error_definition_tests,
        enum_definition_tests,
        variant_definition_tests,
        // --- EXPRESSIONS ---
        function_call_tests,
    };
    return run_all_tests(tests);
}

#endif
