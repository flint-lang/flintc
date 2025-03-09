#pragma once

#include "lexer/token.hpp"
#include "types.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

// Inside this namespace, all the signatures are defined. A signature can
// contain multiple other signatures as well.
namespace Signature {
    using signature = std::vector<std::variant<Token, std::string>>;

    signature combine(std::initializer_list<signature> signatures);

    std::string get_regex_string(const signature &sig);
    std::string stringify(const token_list &tokens);
    bool tokens_contain(const token_list &tokens, const signature &signature);
    bool tokens_match(const token_list &tokens, const signature &signature);
    bool tokens_contain_in_range(const token_list &tokens, const signature &signature, const uint2 &range);
    std::optional<uint2> get_tokens_line_range(const token_list &tokens, unsigned int line);
    std::vector<uint2> get_match_ranges(const token_list &tokens, const signature &signature);
    std::vector<uint2> get_match_ranges_in_range(const token_list &tokens, const signature &signature, const uint2 &range);
    std::optional<uint2> get_next_match_range(const token_list &tokens, const signature &signature);
    signature match_until_signature(const signature &signature);

    std::optional<unsigned int> get_leading_indents(const token_list &tokens, unsigned int line);
    std::optional<uint2> get_line_token_indices(const token_list &tokens, unsigned int line);
    std::optional<uint2> balanced_range_extraction(const token_list &tokens, const signature &inc, const signature &dec);
    std::vector<uint2> balanced_range_extraction_vec(const token_list &tokens, const signature &inc, const signature &dec);

    // --- BASIC SIGNATURES ---
    const signature anytoken = {"#-?..#"};
    const signature type_prim = {"((", TOK_I32, ")|(", TOK_I64, ")|(", TOK_U32, ")|(", TOK_U64, ")|(", TOK_F32, ")|(", TOK_F64, ")|(",
        TOK_FLINT, ")|(", TOK_STR, ")|(", TOK_CHAR, ")|(", TOK_BOOL, "))"};
    const signature literal = {"((", TOK_STR_VALUE, ")|(", TOK_INT_VALUE, ")|(", TOK_FLINT_VALUE, ")|(", TOK_CHAR_VALUE, ")|(", TOK_TRUE,
        ")|(", TOK_FALSE, "))"};
    const signature type = combine({//
        {"("}, type_prim, {"|(", TOK_IDENTIFIER, "))"}});
    const signature operational_binop = {"((", TOK_PLUS, ")|(", TOK_MINUS, ")|(", TOK_MULT, ")|(", TOK_DIV, ")|(", TOK_SQUARE, "))"};
    const signature relational_binop = {"((", TOK_EQUAL_EQUAL, ")|(", TOK_NOT_EQUAL, ")|(", TOK_LESS, ")|(", TOK_LESS_EQUAL, ")|(",
        TOK_GREATER, ")|(", TOK_GREATER_EQUAL, "))"};
    const signature boolean_binop = {"((", TOK_AND, ")|(", TOK_OR, "))"};
    const signature binary_operator = combine({//
        {"("}, operational_binop, {"|"}, relational_binop, {"|"}, boolean_binop, {")"}});
    const signature unary_operator = {"((", TOK_INCREMENT, ")|(", TOK_DECREMENT, ")|(", TOK_NOT, "))"};
    const signature reference = {TOK_IDENTIFIER, "(", TOK_COLON, TOK_COLON, TOK_IDENTIFIER, ")+"};
    const signature args = combine({//
        type, {TOK_IDENTIFIER, "(", TOK_COMMA}, type, {TOK_IDENTIFIER, ")*"}});
    const signature no_prim_args = {TOK_IDENTIFIER, TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, ")*"};
    const signature group = combine({//
        {TOK_LEFT_PAREN}, type, {"(", TOK_COMMA}, type, {")*", TOK_RIGHT_PAREN}});

    // --- DEFINITIONS ---
    const signature use_statement = {TOK_USE, "((", TOK_STR_VALUE, ")|(((", TOK_IDENTIFIER, ")|(", TOK_FLINT, "))(", TOK_DOT,
        TOK_IDENTIFIER, ")*))"};

    const signature function_definition = combine({//
        {"(", TOK_ALIGNED, ")?", "(", TOK_CONST, ")?", TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, "("}, args,
        {")?", TOK_RIGHT_PAREN, "((", TOK_ARROW}, group, {TOK_COLON, ")|(", TOK_ARROW}, type, {TOK_COLON, ")|(", TOK_COLON, "))"}});
    const signature data_definition = {"((", TOK_SHARED, ")|(", TOK_IMMUTABLE, "))?(", TOK_ALIGNED, ")?", TOK_DATA, TOK_IDENTIFIER,
        TOK_COLON};
    const signature func_definition = combine({//
        {TOK_FUNC, TOK_IDENTIFIER, "(", TOK_REQUIRES, TOK_LEFT_PAREN}, no_prim_args, {TOK_RIGHT_PAREN, ")?", TOK_COLON}});
    const signature error_definition = {TOK_ERROR, TOK_IDENTIFIER, "(", TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, ")?", TOK_COLON};
    const signature enum_definition = {TOK_ENUM, TOK_IDENTIFIER, TOK_COLON};
    const signature variant_definition = {TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON};
    const signature test_definition = {TOK_TEST, TOK_STR_VALUE, TOK_COLON};

    // --- ENTITY DEFINITION ---
    const signature entity_definition = combine({//
        {TOK_ENTITY, TOK_IDENTIFIER, "(", TOK_EXTENDS, TOK_LEFT_PAREN}, no_prim_args, {TOK_RIGHT_PAREN, ")?", TOK_COLON}});
    const signature entity_body_data = combine({//
        {TOK_DATA, TOK_COLON, "("}, anytoken, {")*", TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, ")*", TOK_SEMICOLON}});
    const signature entity_body_func = combine({//
        {TOK_FUNC, TOK_COLON, "("}, anytoken, {")*", TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, ")*", TOK_SEMICOLON}});
    const signature entity_body_link = combine({reference, {TOK_ARROW}, reference, {TOK_SEMICOLON}});
    const signature entity_body_links = combine({//
        {TOK_LINK, TOK_COLON, "("}, anytoken, {")*("}, entity_body_link, {"("}, anytoken, {")*)+"}});
    const signature entity_body_constructor = {TOK_IDENTIFIER, TOK_LEFT_PAREN, "(", TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, ")*)?",
        TOK_RIGHT_PAREN, TOK_SEMICOLON};
    const signature entity_body = combine({//
        {"("}, entity_body_data, {")?("}, anytoken, {")*("}, entity_body_func, {")?("}, anytoken, {")*("}, entity_body_links, {")?("},
        anytoken, {")*"}, entity_body_constructor});

    // --- STATEMENTS ---
    const signature declaration_without_initializer = combine({type, {TOK_IDENTIFIER, TOK_SEMICOLON}});
    const signature declaration_explicit = combine({type, {TOK_IDENTIFIER, TOK_EQUAL}});
    const signature declaration_infered = {TOK_IDENTIFIER, TOK_COLON_EQUAL};
    const signature assignment = {TOK_IDENTIFIER, TOK_EQUAL};
    const signature for_loop = combine({//
        {TOK_FOR}, match_until_signature({TOK_SEMICOLON}), match_until_signature({TOK_SEMICOLON}), match_until_signature({TOK_COLON})});
    const signature enhanced_for_loop = combine({//
        {TOK_FOR, "((", TOK_UNDERSCORE, ")|(", TOK_IDENTIFIER, "))", TOK_COMMA, "((", TOK_UNDERSCORE, ")|(", TOK_IDENTIFIER, "))", TOK_IN},
        match_until_signature({TOK_COLON})});
    const signature par_for_loop = combine({{TOK_PARALLEL}, enhanced_for_loop});
    const signature while_loop = combine({{TOK_WHILE}, match_until_signature({TOK_COLON})});
    const signature if_statement = combine({{TOK_IF}, match_until_signature({TOK_COLON})});
    const signature else_if_statement = combine({{TOK_ELSE, TOK_IF}, match_until_signature({TOK_COLON})});
    const signature else_statement = combine({{TOK_ELSE}, match_until_signature({TOK_COLON})});
    const signature return_statement = combine({{TOK_RETURN}, match_until_signature({TOK_SEMICOLON})});
    const signature throw_statement = combine({{TOK_THROW}, match_until_signature({TOK_SEMICOLON})});

    // --- EXPRESSIONS ---
    const signature expression = combine({{"("}, anytoken, {")*"}});
    const signature function_call = combine({{TOK_IDENTIFIER, TOK_LEFT_PAREN, "("}, expression, {")?", TOK_RIGHT_PAREN}});
    const signature type_cast = combine({type_prim, {TOK_LEFT_PAREN, "("}, expression, {")", TOK_RIGHT_PAREN}});
    const signature bin_op_expr = combine({expression, binary_operator, expression});
    const signature unary_op_expr = combine({{"(("}, expression, unary_operator, {")|("}, unary_operator, expression, {"))"}});
    const signature literal_expr = combine({//
        {"(("}, literal, {"("}, binary_operator, literal, {")*)|("}, unary_operator, literal, {")|("}, literal, unary_operator, {"))"}});
    const signature variable_expr = {TOK_IDENTIFIER, "(?!", TOK_LEFT_PAREN, ")"};

    // --- ERROR HANDLING --- (requires function_call to be defined)
    const signature catch_statement = combine({function_call, {TOK_CATCH, "(", TOK_IDENTIFIER, ")?", TOK_COLON}});
} // namespace Signature
