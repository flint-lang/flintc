#pragma once

#include "debug.hpp"
#include "lexer/token.hpp"
#include "parser/signature.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

// --- MATCH TEST IF STATEMENT ---
namespace {
    TestResult test_match_else() {
        TestResult test_result;
        Debug::print_tree_row({Debug::BRANCH}, &test_result);
        test_result.append_test_name("IF_STATEMENT TESTS:", true);
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_match_else", false);
        // if true:
        token_list tokens = create_token_vector({//
            TOK_ELSE, TOK_COLON});
        bool result = Signature::tokens_match(tokens, ESignature::ELSE_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- CONTAIN TEST IF STATEMENT ---
namespace {
    TestResult test_contain_else() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::BRANCH}, &test_result);
        test_result.append_test_name("test_contain_else", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ELSE, TOK_COLON, TOK_EOL});
        bool result = Signature::tokens_contain(tokens, ESignature::ELSE_STATEMENT);
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace
// --- EXTRACT TEST IF STATEMENT ---
namespace {
    TestResult test_extract_else() {
        TestResult test_result;
        Debug::print_tree_row({Debug::VERT, Debug::SINGLE}, &test_result);
        test_result.append_test_name("test_extract_else", false);
        token_list tokens = create_token_vector({//
            TOK_INDENT, TOK_ELSE, TOK_COLON, TOK_EOL});
        std::vector<uint2> result_vec = Signature::get_match_ranges(tokens, ESignature::ELSE_STATEMENT);
        bool result = !result_vec.empty() && result_vec.at(0).first == 1 && result_vec.at(0).second == 3;
        test_result.ok_or_not(result);
        if (!result) {
            test_result.increment();
        }
        return test_result;
    }
} // namespace

function_list get_else_statement_tests() {
    function_list else_statement_tests = {
        // Match Tests
        test_match_else,
        // Contain Tests
        test_contain_else,
        // Extract Tests
        test_extract_else,
    };
    return else_statement_tests;
}
