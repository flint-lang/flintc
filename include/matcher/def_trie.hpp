#pragma once

#include "matcher.hpp"
#include "parser/type/opaque_type.hpp"
#include "trie.hpp"

/// @class `DefTrie`
/// @brief This class is an implementation of the trie to match all definitions
class DefTrie : public Trie<DefTrie> {
  public:
    /// @enum `Pattern`
    /// @brief An enum of all possible patterns a definition could have
    enum class Pattern {
        ANNOTATION,
        USE,
        TYPE_ALIAS,
        EXTERN_FUNCTION,
        OPAQUE,
        FUNCTION,
        TEST,
        DATA,
        FUNC,
        INTERFACE,
        OBJECT,
        ENUM,
        ERROR,
        VARIANT,
    };

    /// @function `init`
    /// @brief Seeds the root node of the def trie with its full pattern range (every pattern from the first to the last enumerator,
    /// inclusive)
    static void init(Trie<DefTrie>::RootNode<Pattern> &root) {
        Trie<DefTrie>::RootNode<Pattern>::init(root, Pattern::ANNOTATION, Pattern::VARIANT);
    }

    /// @var `matchers`
    /// @brief A map which maps all possible patterns to their matching functions and their trie affinity
    static inline const Trie<DefTrie>::matchers_map<Pattern> matchers = {
        {
            Pattern::ANNOTATION,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::token(TOK_ANNOTATION)); },
            },
        },
        {
            Pattern::USE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::use_statement); },
            },
        },
        {
            Pattern::TYPE_ALIAS,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::type_alias); },
            },
        },
        {
            Pattern::EXTERN_FUNCTION,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::extern_function_declaration); },
            },
        },
        {
            Pattern::OPAQUE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const bool is_opaque_keyword = tokens.first->token == TOK_OPAQUE;
                    const bool is_opaque_type = tokens.first->token == TOK_TYPE           //
                        && tokens.first->type->get_variation() == Type::Variation::OPAQUE //
                        && !tokens.first->type->as<OpaqueType>()->name.has_value();
                    return (is_opaque_keyword || is_opaque_type) && Matcher::tokens_start_with(tokens, Matcher::opaque_definition);
                },
            },
        },
        {
            Pattern::FUNCTION,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::function_definition); },
            },
        },
        {
            Pattern::TEST,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::test_definition); },
            },
        },
        {
            Pattern::DATA,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::data_definition); },
            },
        },
        {
            Pattern::FUNC,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::func_definition); },
            },
        },
        {
            Pattern::INTERFACE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::interface_definition); },
            },
        },
        {
            Pattern::OBJECT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::object_definition); },
            },
        },
        {
            Pattern::ENUM,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::enum_definition); },
            },
        },
        {
            Pattern::ERROR,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::error_definition); },
            },
        },
        {
            Pattern::VARIANT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::variant_definition); },
            },
        },
    };
};
