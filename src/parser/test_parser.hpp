#ifndef __TEST_PARSER_HPP__
#define __TEST_PARSER_HPP__

#include "signature.hpp"
#include "../test_utils.hpp"

#include <string>

// --- THE SIGNATURE TESTS ---

// --- PRIMARY TESTS ---
// --- MATCH TEST PRIMARY ---
namespace {
    int test_match_prim() {
        print_test_name(2, "PRIMARY TESTS:", true);
        print_test_name(3, "PRIMARY_MATCH:", true);
        return 0;
    }

    int test_match_prim_int() {
        print_test_name(4, "test_match_prim_int", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INT}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_prim_flint() {
        print_test_name(4, "test_match_prim_flint", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_FLINT}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_prim_str() {
        print_test_name(4, "test_match_prim_str", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_STR}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_prim_char() {
            print_test_name(4, "test_match_char_int", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_CHAR}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_prim_bool() {
        print_test_name(4, "test_match_prim_bool", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_BOOL}
        );
        bool result = Signature::tokens_match(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST PRIMARY ---
namespace {
    int test_contain_prim() {
        print_test_name(3, "PRIMARY_CONTAIN:", true);
        return 0;
    }

    int test_contain_prim_int() {
        print_test_name(4, "test_contain_prim_int", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_INT, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_prim_flint() {
        print_test_name(4, "test_contain_prim_flint", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_FLINT, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_prim_str() {
        print_test_name(4, "test_contain_prim_str", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_STR, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_prim_char() {
        print_test_name(4, "test_contain_prim_char", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_CHAR, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_prim_bool() {
        print_test_name(4, "test_contain_prim_bool", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_EOL, TOK_BOOL, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type_prim);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST PRIMARY ---
namespace {
    int test_extract_prim() {
        print_test_name(3, "PRIMARY_EXTRACT:", true);
        return 0;
    }

    int test_extract_prim_int() {
        print_test_name(4, "test_extract_prim_int", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_prim_flint() {
        print_test_name(4, "test_extract_prim_flint", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_FLINT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_prim_str() {
        print_test_name(4, "test_extract_prim_str", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_STR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_prim_char() {
        print_test_name(4, "test_extract_prim_char", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_CHAR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_prim_bool() {
        print_test_name(4, "test_extract_prim_bool", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_BOOL, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type_prim);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 2 && result_vec.at(0).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- TYPE TESTS ---
// --- MATCH TEST TYPE ---
namespace {
    int test_match_type() {
        print_test_name(2, "TYPE TESTS:", true);
        print_test_name(3, "TYPE_MATCH:", true);
        return 0;
    }

    int test_match_type_int() {
        print_test_name(4, "test_match_type_int", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INT}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_flint() {
        print_test_name(4, "test_match_type_flint", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_FLINT}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_str() {
        print_test_name(4, "test_match_type_str", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_STR}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_char() {
        print_test_name(4, "test_match_type_char", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_CHAR}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_bool() {
        print_test_name(4, "test_match_type_bool", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_BOOL}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_type_identifier() {
        print_test_name(4, "test_match_type_identifier", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST TYPE ---
namespace {
    int test_contain_type() {
        print_test_name(3, "TYPE_CONTAIN:", true);
        return 0;
    }

    int test_contain_type_int() {
        print_test_name(4, "test_contain_type_int", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_COLON, TOK_INT, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_flint() {
        print_test_name(4, "test_contain_type_flint", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_COLON, TOK_FLINT, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_str() {
        print_test_name(4, "test_contain_type_str", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_COLON, TOK_STR, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_char() {
        print_test_name(4, "test_contain_type_char", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_COLON, TOK_CHAR, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_bool() {
        print_test_name(4, "test_contain_type_bool", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_COLON, TOK_BOOL, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_type_identifier() {
        print_test_name(4, "test_contain_type_identifier", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_COLON, TOK_IDENTIFIER, TOK_DATA}
        );
        bool result = Signature::tokens_contain(tokens, Signature::type);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// -- EXTRACT TEST TYPE ---
namespace {
    int test_extract_type() {
        print_test_name(3, "TYPE_EXTRACT:", true);
        return 0;
    }

    int test_extract_type_int() {
        print_test_name(4, "test_extract_type_int", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_flint() {
        print_test_name(4, "test_extract_type_flint", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_FLINT, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_str() {
        print_test_name(4, "test_extract_type_str", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_STR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_char() {
        print_test_name(4, "test_extract_type_char", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_CHAR, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_bool() {
        print_test_name(4, "test_extract_type_bool", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_BOOL, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type);
        bool result = !result_vec.empty() &&
            result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_type_identifier() {
        print_test_name(4, "test_extract_type_identifier", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_EQUAL, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::type);
        bool result = !result_vec.empty();
        result = result && result_vec.at(0).first == 1 && result_vec.at(0).second == 2;
        result = result && result_vec.at(1).first == 2 && result_vec.at(1).second == 3;
        result = result && result_vec.at(2).first == 4 && result_vec.at(2).second == 5;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- REFERENCE TESTS ---
// --- MATCH TEST REFERENCE ---
namespace {
    int test_match_reference() {
        print_test_name(2, "REFERENCE TESTS:", true);
        print_test_name(3, "REFERENCE_MATCH:", true);
        return 0;
    }

    int test_match_reference_single() {
        print_test_name(4, "test_match_reference_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::reference);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_reference_multiple() {
        print_test_name(4, "test_match_reference_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::reference);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST REFERENCE ---
namespace {
    int test_contain_reference() {
        print_test_name(3, "REFERENCE_CONTAIN:", true);
        return 0;
    }

    int test_contain_reference_single() {
        print_test_name(4, "test_contain_reference_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::reference);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_reference_multiple() {
        print_test_name(4, "test_contain_reference_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::reference);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST REFERENCE ---
namespace {
    int test_extract_reference() {
        print_test_name(3, "REFERENCE_EXTRACT:", true);
        return 0;
    }

    int test_extract_reference_single() {
        print_test_name(4, "test_extract_reference_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::reference);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 4 && result_vec.at(0).second == tokens.size() - 1;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_reference_multiple() {
        print_test_name(4, "test_extract_reference_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IDENTIFIER, TOK_INT, TOK_EQUAL, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_COLON, TOK_COLON, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::reference);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 4 && result_vec.at(0).second == tokens.size() - 1;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- ARGS TESTS ---
// --- MATCH TEST ARGS ---
namespace {
    int test_match_args() {
        print_test_name(2, "ARGS TESTS:", true);
        print_test_name(3, "ARGS_MATCH:", true);
        return 0;
    }

    int test_match_args_single() {
        print_test_name(4, "test_match_args_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::args);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_args_multiple() {
        print_test_name(4, "test_match_args_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::args);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST ARGS ---
namespace {
    int test_contain_args() {
        print_test_name(3, "ARGS_CONTAIN:", true);
        return 0;
    }

    int test_contain_args_single() {
        print_test_name(4, "test_contain_args_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::args);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_args_multiple() {
        print_test_name(4, "test_contain_args_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::args);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST ARGS ---
namespace {
    int test_extract_args() {
        print_test_name(3, "ARGS_EXTRACT:", true);
        return 0;
    }

    int test_extract_args_single() {
        print_test_name(4, "test_extract_args_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::args);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 3 && result_vec.at(0).second == 5;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_args_multiple() {
        print_test_name(4, "test_extract_args_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::args);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 3 && result_vec.at(0).second == 8;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- GROUP TESTS ---
// --- MATCH TEST GROUP ---
namespace {
    int test_match_group() {
        print_test_name(2, "GROUP TESTS:", true);
        print_test_name(3, "GROUP_MATCH:", true);
        return 0;
    }

    int test_match_group_single() {
        print_test_name(4, "test_match_group_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_LEFT_PAREN, TOK_INT, TOK_RIGHT_PAREN}
        );
        bool result = Signature::tokens_match(tokens, Signature::group);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_group_multiple() {
        print_test_name(4, "test_match_group_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN}
        );
        bool result = Signature::tokens_match(tokens, Signature::group);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST GROUP ---
namespace {
    int test_contain_group() {
        print_test_name(3, "GROUP_CONTAIN:", true);
        return 0;
    }

    int test_contain_group_single() {
        print_test_name(4, "test_contain_group_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::group);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_group_multiple() {
        print_test_name(4, "test_contain_group_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::group);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST GROUP ---
namespace {
    int test_extract_group() {
        print_test_name(3, "GROUP_EXTRACT:", true);
        return 0;
    }

    int test_extract_group_single() {
        print_test_name(4, "test_extract_group_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::group);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 7 && result_vec.at(0).second == tokens.size() - 1;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_group_multiple() {
        print_test_name(4, "test_extract_group_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::group);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 10 && result_vec.at(0).second == 15;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- USE STATEMENT TESTS ---
// --- MATCH TEST USE STATEMENT ---
namespace {
    int test_match_use_statement() {
        print_test_name(2, "USE_STATEMENT TESTS:", true);
        print_test_name(3, "USE_STATEMENT_MATCH:", true);
        return 0;
    }

    int test_match_use_statement_string() {
        print_test_name(4, "test_match_use_statement_string", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_USE, TOK_STR_VALUE}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_package_single() {
        print_test_name(4, "test_match_use_statement_package_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_USE, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_package_dual() {
        print_test_name(4, "test_match_use_statement_package_dual", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_use_statement_package_multiple() {
        print_test_name(4, "test_match_use_statement_package_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER}
        );
        bool result = Signature::tokens_match(tokens, Signature::use_statement);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST USE STATEMENT ---
namespace {
    int test_contain_use_statement() {
        print_test_name(3, "USE_STATEMENT_CONTAIN:", true);
        return 0;
    }

    int test_contain_use_statement_string() {
        print_test_name(4, "test_contain_use_statement_string", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_STR_VALUE, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_package_single() {
        print_test_name(4, "test_contain_use_statement_package_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_package_dual() {
        print_test_name(4, "test_contain_use_statement_package_dual", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_use_statement_package_multiple() {
        print_test_name(4, "test_contain_use_statement_package_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        bool result = Signature::tokens_contain(tokens, Signature::use_statement);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST USE STATEMENT ---
namespace {
    int test_extract_use_statement() {
        print_test_name(3, "USE_STATEMENT_EXTRACT:", true);
        return 0;
    }

    int test_extract_use_statement_string() {
        print_test_name(4, "test_extract_use_statement_string", false);
        std::vector<TokenContext> tokens = create_token_vector(
           {TOK_INDENT, TOK_USE, TOK_STR_VALUE, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_package_single() {
        print_test_name(4, "test_extract_use_statement_package_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_package_dual() {
        print_test_name(4, "test_extract_use_statement_package_dual", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_use_statement_package_multiple() {
        print_test_name(4, "test_extract_use_statement_package_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_USE, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, TOK_SEMICOLON}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::use_statement);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- FUNCTION DEFINITION TESTS ---
// --- MATCH TEST FUNCTION DEFINITION ---
namespace {
    int test_match_function_definition() {
        print_test_name(2, "FUNCTION_DEFINITION TESTS:", true);
        print_test_name(3, "FUNCTION_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_function_definition_const() {
        print_test_name(4, "test_match_function_definition_const", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_aligned() {
        print_test_name(4, "test_match_function_definition_aligned", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_aligned_const() {
        print_test_name(4, "test_match_function_definition_aligned_const", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_0arg_0return() {
        print_test_name(4, "test_match_function_definition_0arg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_1arg_0return() {
        print_test_name(4, "test_match_function_definition_1arg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_0arg_1return() {
        print_test_name(4, "test_match_function_definition_0arg_1return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_1arg_1return() {
        print_test_name(4, "test_match_function_definition_1arg_1return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_narg_0return() {
        print_test_name(4, "test_match_function_definition_narg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_0arg_nreturn() {
        print_test_name(4, "test_match_function_definition_0arg_nreturn", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_function_definition_narg_nreturn() {
        print_test_name(4, "test_match_function_definition_narg_nreturn", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST FUNCTION DEFINITION ---
namespace {
    int test_contain_function_definition() {
        print_test_name(3, "FUNCTION_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_function_definition_const() {
        print_test_name(4, "test_contain_function_definition_const", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_aligned() {
        print_test_name(4, "test_contain_function_definition_aligned", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_aligned_const() {
        print_test_name(4, "test_contain_function_definition_aligned_const", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_0arg_0return() {
        print_test_name(4, "test_contain_function_definition_0arg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_1arg_0return() {
        print_test_name(4, "test_contain_function_definition_1arg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_0arg_1return() {
        print_test_name(4, "test_contain_function_definition_0arg_1return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_1arg_1return() {
        print_test_name(4, "test_contain_function_definition_1arg_1return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_narg_0return() {
        print_test_name(4, "test_contain_function_definition_narg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_0arg_nreturn() {
        print_test_name(4, "test_contain_function_definition_0arg_nreturn", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_function_definition_narg_nreturn() {
        print_test_name(4, "test_contain_function_definition_narg_nreturn", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::function_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST FUNCTION DEFINITION ---
namespace {
    int test_extract_function_definition() {
        print_test_name(3, "FUNCTION_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_function_definition_const() {
        print_test_name(4, "test_extract_function_definition_const", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_aligned() {
        print_test_name(4, "test_extract_function_definition_aligned", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 7;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_aligned_const() {
        print_test_name(4, "test_extract_function_definition_aligned_const", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_CONST, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_0arg_0return() {
        print_test_name(4, "test_extract_function_definition_0arg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
           {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 6;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_1arg_0return() {
        print_test_name(4, "test_extract_function_definition_1arg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_0arg_1return() {
        print_test_name(4, "test_extract_function_definition_0arg_1return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 8;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_1arg_1return() {
        print_test_name(4, "test_extract_function_definition_1arg_1return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_INT, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 10;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_narg_0return() {
        print_test_name(4, "test_extract_function_definition_narg_0return", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 11;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_0arg_nreturn() {
        print_test_name(4, "test_extract_function_definition_0arg_nreturn", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 12;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_function_definition_narg_nreturn() {
        print_test_name(4, "test_extract_function_definition_narg_nreturn", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, TOK_INT, TOK_IDENTIFIER, TOK_COMMA, TOK_FLINT, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_ARROW, TOK_LEFT_PAREN, TOK_INT, TOK_COMMA, TOK_FLINT, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::function_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 17;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- DATA DEFINITION TESTS ---
// --- MATCH TEST DATA DEFINITION ---
namespace {
    int test_match_data_definition() {
        print_test_name(2, "DATA_DEFINITION TESTS:", true);
        print_test_name(3, "DATA_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_data_definition_normal() {
        print_test_name(4, "test_match_data_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_DATA, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_data_definition_shared() {
        print_test_name(4, "test_match_data_definition_shared", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_data_definition_immutable() {
        print_test_name(4, "test_match_data_definition_immutable", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_data_definition_aligned() {
        print_test_name(4, "test_match_data_definition_aligned", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::data_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST DATA DEFINITION ---
namespace {
    int test_contain_data_definition() {
        print_test_name(3, "DATA_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_data_definition_normal() {
        print_test_name(4, "test_contain_data_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_data_definition_shared() {
        print_test_name(4, "test_contain_data_definition_shared", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_data_definition_immutable() {
        print_test_name(4, "test_contain_data_definition_immutable", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_data_definition_aligned() {
        print_test_name(4, "test_contain_data_definition_aligned", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::data_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST DATA DEFINITION ---
namespace {
    int test_extract_data_definition() {
        print_test_name(3, "DATA_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_data_definition_normal() {
        print_test_name(4, "test_extract_data_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
           {TOK_INDENT, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::data_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_data_definition_shared() {
        print_test_name(4, "test_extract_data_definition_shared", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_SHARED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::data_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_data_definition_immutable() {
        print_test_name(4, "test_extract_data_definition_immutable", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_IMMUTABLE, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::data_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_data_definition_aligned() {
        print_test_name(4, "test_extract_data_definition_aligned", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ALIGNED, TOK_DATA, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::data_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 5;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- FUNC DEFINITION TESTS ---
// --- MATCH TEST FUNC DEFINITION ---
namespace {
    int test_match_func_definition() {
        print_test_name(2, "FUNC_DEFINITION TESTS:", true);
        print_test_name(3, "FUNC_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_func_definition_normal() {
        print_test_name(4, "test_match_func_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_FUNC, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::func_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_func_definition_requires_single() {
        print_test_name(4, "test_match_func_definition_requires_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::func_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_func_definition_requires_multiple() {
        print_test_name(4, "test_match_func_definition_requires_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::func_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST FUNC DEFINITION ---
namespace {
    int test_contain_func_definition() {
        print_test_name(3, "FUNC_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_func_definition_normal() {
        print_test_name(4, "test_contain_func_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::func_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_func_definition_requires_single() {
        print_test_name(4, "test_contain_func_definition_requires_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::func_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_func_definition_requires_multiple() {
        print_test_name(4, "test_contain_func_definition_requires_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::func_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST FUNC DEFINITION ---
namespace {
    int test_extract_func_definition() {
        print_test_name(3, "FUNC_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_func_definition_normal() {
        print_test_name(4, "test_extract_func_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
           {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::func_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_func_definition_requires_single() {
        print_test_name(4, "test_extract_func_definition_requires_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::func_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 9;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_func_definition_requires_multiple() {
        print_test_name(4, "test_extract_func_definition_requires_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_FUNC, TOK_IDENTIFIER, TOK_REQUIRES, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::func_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 12;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

// --- ENTITY DEFINITION TESTS ---
// --- MATCH TEST ENTITY DEFINITION ---
namespace {
    int test_match_entity_definition() {
        print_test_name(2, "ENTITY_DEFINITION TESTS:", true);
        print_test_name(3, "ENTITY_DEFINITION_MATCH:", true);
        return 0;
    }

    int test_match_entity_definition_normal() {
        print_test_name(4, "test_match_entity_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_ENTITY, TOK_IDENTIFIER, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::entity_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_entity_definition_extends_single() {
        print_test_name(4, "test_match_entity_definition_extends_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::entity_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_match_entity_definition_extends_multiple() {
        print_test_name(4, "test_match_entity_definition_extends_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON}
        );
        bool result = Signature::tokens_match(tokens, Signature::entity_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- CONTAIN TEST ENTITY DEFINITION ---
namespace {
    int test_contain_entity_definition() {
        print_test_name(3, "ENTITY_DEFINITION_CONTAIN:", true);
        return 0;
    }

    int test_contain_entity_definition_normal() {
        print_test_name(4, "test_contain_entity_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::entity_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_entity_definition_extends_single() {
        print_test_name(4, "test_contain_entity_definition_extends_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::entity_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_contain_entity_definition_extends_multiple() {
        print_test_name(4, "test_contain_entity_definition_extends_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        bool result = Signature::tokens_contain(tokens, Signature::entity_definition);
        ok_or_not(result);
        return result ? 0 : 1;
    }
}
// --- EXTRACT TEST ENTITY DEFINITION ---
namespace {
    int test_extract_entity_definition() {
        print_test_name(3, "ENTITY_DEFINITION_EXTRACT:", true);
        return 0;
    }

    int test_extract_entity_definition_normal() {
        print_test_name(4, "test_extract_entity_definition_normal", false);
        std::vector<TokenContext> tokens = create_token_vector(
           {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::entity_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 4;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_entity_definition_extends_single() {
        print_test_name(4, "test_extract_entity_definition_extends_single", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::entity_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 9;
        ok_or_not(result);
        return result ? 0 : 1;
    }

    int test_extract_entity_definition_extends_multiple() {
        print_test_name(4, "test_extract_entity_definition_extends_multiple", false);
        std::vector<TokenContext> tokens = create_token_vector(
            {TOK_INDENT, TOK_ENTITY, TOK_IDENTIFIER, TOK_EXTENDS, TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, TOK_RIGHT_PAREN, TOK_COLON, TOK_EOL}
        );
        std::vector<std::pair<uint, uint>> result_vec = Signature::extract_matches(tokens, Signature::entity_definition);
        bool result = !result_vec.empty() &&
            result_vec.at(0).first == 1 && result_vec.at(0).second == 12;
        ok_or_not(result);
        return result ? 0 : 1;
    }
}

int test_parser() {
    print_test_name(0, "PARSER_TESTS:", true);
    print_test_name(1, "SIGNATURE_TESTS:", true);
    int failure_sum = 0;

    // Primary Tests
    failure_sum += run_all_tests({
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
    });
    // Type Tests
    failure_sum += run_all_tests({
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
    });
    // Reference Tests
    failure_sum += run_all_tests({
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
    });
    // Args Tests
    failure_sum += run_all_tests({
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
    });
    // Group Tests
    failure_sum += run_all_tests({
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
    });
    // Use Statement Tests
    failure_sum += run_all_tests({
        // Match Tests
        test_match_use_statement,
        test_match_use_statement_string,
        test_match_use_statement_package_single,
        test_match_use_statement_package_dual,
        test_match_use_statement_package_multiple,
        // Contain Tests
        test_contain_use_statement,
        test_contain_use_statement_string,
        test_contain_use_statement_package_single,
        test_contain_use_statement_package_dual,
        test_contain_use_statement_package_multiple,
        // Extract Tests
        test_extract_use_statement,
        test_extract_use_statement_string,
        test_extract_use_statement_package_single,
        test_extract_use_statement_package_dual,
        test_extract_use_statement_package_multiple,
    });
    // Function Definition Tests
    failure_sum += run_all_tests({
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
    });
    // Data Definition Tests
    failure_sum += run_all_tests({
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
    });
    // Func Definition Tests
    failure_sum += run_all_tests({
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
    });
    // Entity Definition Tests
    failure_sum += run_all_tests({
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
    });

    return failure_sum;
}

#endif
