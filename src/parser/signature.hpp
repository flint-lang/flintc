#ifndef __SIGNATURE_HPP__
#define __SIGNATURE_HPP__

#include "../types.hpp"

#include <vector>
#include <string>
#include <optional>
#include <utility>

// Inside this namespace, all the signatures are defined. A signature can contain multiple other signatures as well.
namespace Signature {

    signature combine(std::initializer_list<signature> signatures);

    std::string get_regex_string(const signature &sig);
    std::string stringify(const token_list &tokens);
    bool tokens_contain(const token_list &tokens, const signature &signature);
    bool tokens_match(const token_list &tokens, const signature &signature);
    std::vector<std::pair<uint, uint>> extract_matches(const token_list &tokens, const signature &signature);

    std::optional<uint> get_leading_indents(const token_list &tokens, uint line);
    std::optional<std::pair<uint, uint>> get_line_token_indices(const token_list &tokens, uint line);

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

    const signature use_statement = {TOK_USE, "((", TOK_STR_VALUE, ")|(", TOK_IDENTIFIER, "(", TOK_DOT, TOK_IDENTIFIER, ")*))"};
    const signature function_definition = combine({
        {"(", TOK_ALIGNED, ")?", "(", TOK_CONST, ")?", TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, "("}, args, {")?", TOK_RIGHT_PAREN, "((", TOK_ARROW}, group, {TOK_COLON, ")|(", TOK_ARROW}, type, {TOK_COLON, ")|(", TOK_COLON, "))"}
    });
    const signature data_definition = {"((", TOK_SHARED, ")|(", TOK_IMMUTABLE, ")|(", TOK_ALIGNED, "))?", TOK_DATA, TOK_IDENTIFIER, TOK_COLON};
    const signature func_definition = combine({
        {TOK_FUNC, TOK_IDENTIFIER, "(", TOK_REQUIRES, TOK_LEFT_PAREN}, no_prim_args, {TOK_RIGHT_PAREN, ")?", TOK_COLON}
    });
    const signature entity_definition = combine({
        {TOK_ENTITY, TOK_IDENTIFIER, "(", TOK_EXTENDS, TOK_LEFT_PAREN}, no_prim_args, {TOK_RIGHT_PAREN, ")?", TOK_COLON}
    });
    const signature error_definition = {TOK_ERROR, TOK_IDENTIFIER, "(", TOK_IDENTIFIER,")?", TOK_COLON};
    const signature enum_definition = {TOK_ENUM, TOK_IDENTIFIER, TOK_COLON};
    const signature variant_definition = {TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON};


    const signature initialization = combine({
        type, {TOK_IDENTIFIER, TOK_EQUAL}
    });
    const signature assignment = {TOK_IDENTIFIER, TOK_EQUAL};
}

#endif
