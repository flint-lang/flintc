#ifndef __SIGNATURE_HPP__
#define __SIGNATURE_HPP__

#include "../types.hpp"

#include <vector>
#include <string>
#include <optional>
#include <utility>
#include <variant>

// Inside this namespace, all the signatures are defined. A signature can contain multiple other signatures as well.
namespace Signature {
    using signature = std::vector<std::variant<Token, std::string>>;

    signature combine(std::initializer_list<signature> signatures);

    std::string get_regex_string(const signature &sig);
    std::string stringify(const token_list &tokens);
    bool tokens_contain(const token_list &tokens, const signature &signature);
    bool tokens_match(const token_list &tokens, const signature &signature);
    std::vector<std::pair<unsigned int, unsigned int>> get_match_ranges(const token_list &tokens, const signature &signature);
    signature match_until_signature(const signature &signature);

    std::optional<unsigned int> get_leading_indents(const token_list &tokens, unsigned int line);
    std::optional<std::pair<unsigned int, unsigned int>> get_line_token_indices(const token_list &tokens, unsigned int line);

    // --- BASIC SIGNATURES ---
    const signature anytoken = {"#-?..#"};
    const signature type_prim = {"((", TOK_INT, ")|(", TOK_FLINT, ")|(", TOK_STR, ")|(", TOK_CHAR, ")|(", TOK_BOOL, "))"};
    const signature type = combine({
        {"("}, type_prim, {"|(", TOK_IDENTIFIER, "))"}
    });
    const signature reference = {TOK_IDENTIFIER, "(", TOK_COLON, TOK_COLON, TOK_IDENTIFIER, ")+"};
    const signature args = combine({
        type, {TOK_IDENTIFIER, "(", TOK_COMMA}, type , {TOK_IDENTIFIER, ")*"}
    });
    const signature no_prim_args = {TOK_IDENTIFIER, TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, ")*"};
    const signature group = combine({
        {TOK_LEFT_PAREN}, type, {"(", TOK_COMMA}, type, {")*", TOK_RIGHT_PAREN}
    });

    // --- DEFINITIONS ---
    const signature use_statement = {TOK_USE, "((", TOK_STR_VALUE, ")|(((", TOK_IDENTIFIER, ")|(", TOK_FLINT, "))(", TOK_DOT, TOK_IDENTIFIER, ")*))"};
    const signature function_definition = combine({
        {"(", TOK_ALIGNED, ")?", "(", TOK_CONST, ")?", TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, "("}, args, {")?", TOK_RIGHT_PAREN, "((", TOK_ARROW}, group, {TOK_COLON, ")|(", TOK_ARROW}, type, {TOK_COLON, ")|(", TOK_COLON, "))"}
    });
    const signature data_definition = {"((", TOK_SHARED, ")|(", TOK_IMMUTABLE, "))?(", TOK_ALIGNED, ")?", TOK_DATA, TOK_IDENTIFIER, TOK_COLON};
    const signature func_definition = combine({
        {TOK_FUNC, TOK_IDENTIFIER, "(", TOK_REQUIRES, TOK_LEFT_PAREN}, no_prim_args, {TOK_RIGHT_PAREN, ")?", TOK_COLON}
    });
    const signature error_definition = {TOK_ERROR, TOK_IDENTIFIER, "(", TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, ")?", TOK_COLON};
    const signature enum_definition = {TOK_ENUM, TOK_IDENTIFIER, TOK_COLON};
    const signature variant_definition = {TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON};

    // --- ENTITY DEFINITION ---
    const signature entity_definition = combine({
        {TOK_ENTITY, TOK_IDENTIFIER, "(", TOK_EXTENDS, TOK_LEFT_PAREN}, no_prim_args, {TOK_RIGHT_PAREN, ")?", TOK_COLON}
    });
    const signature entity_body_data = combine({
        {TOK_DATA, TOK_COLON, "("}, anytoken, {")*", TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, ")*", TOK_SEMICOLON}
    });
    const signature entity_body_func = combine({
        {TOK_FUNC, TOK_COLON, "("}, anytoken, {")*", TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, ")*", TOK_SEMICOLON}
    });
    const signature entity_body_link = combine({
        reference, {TOK_ARROW}, reference, {TOK_SEMICOLON}
    });
    const signature entity_body_links = combine({
        {TOK_LINK, TOK_COLON, "("}, anytoken, {")*("}, entity_body_link, {"("}, anytoken,{")*)+"}
    });
    const signature entity_body_constructor = {TOK_IDENTIFIER, TOK_LEFT_PAREN, "(", TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, ")*)?", TOK_RIGHT_PAREN, TOK_SEMICOLON};
    const signature entity_body = combine({
        {"("}, entity_body_data, {")?("}, anytoken, {")*("}, entity_body_func, {")?("}, anytoken, {")*("}, entity_body_links, {")?("}, anytoken, {")*"}, entity_body_constructor
    });

    // --- STATEMENTS ---
    const signature declaration_explicit = combine({
        type, {TOK_IDENTIFIER, TOK_EQUAL}
    });
    const signature declaration_infered = {TOK_IDENTIFIER, TOK_COLON_EQUAL};
    const signature assignment = {TOK_IDENTIFIER, TOK_EQUAL};
    const signature for_loop = combine({
        {TOK_FOR}, match_until_signature({TOK_SEMICOLON}), match_until_signature({TOK_SEMICOLON}), match_until_signature({TOK_COLON})
    });
    const signature enhanced_for_loop = combine({
        {TOK_FOR, "((", TOK_UNDERSCORE, ")|(", TOK_IDENTIFIER, "))", TOK_COMMA, "((", TOK_UNDERSCORE, ")|(", TOK_IDENTIFIER, "))", TOK_IN}, match_until_signature({TOK_COLON})
    });
    const signature par_for_loop = combine({
        {TOK_PAR_FOR, "((", TOK_UNDERSCORE, ")|(", TOK_IDENTIFIER, "))", TOK_COMMA, "((", TOK_UNDERSCORE, ")|(", TOK_IDENTIFIER, "))", TOK_IN}, match_until_signature({TOK_COLON})
    });
    const signature while_loop = combine({
       {TOK_WHILE}, match_until_signature({TOK_COLON})
    });
    const signature if_statement = combine({
       {TOK_IF}, match_until_signature({TOK_COLON})
    });
    const signature else_if_statement = combine({
        {TOK_ELSE, TOK_IF}, match_until_signature({TOK_COLON})
    });
    const signature else_statement = combine({
        {TOK_ELSE}, match_until_signature({TOK_COLON})
    });
    const signature return_statement = combine({
       {TOK_RETURN}, match_until_signature({TOK_SEMICOLON})
    });
}

#endif
