#pragma once

#include "matcher.hpp"
#include "trie.hpp"

/// @class `ScopedStmtTrie`
/// @brief This class is an implementation of the trie to match all scoped statements
class ScopedStmtTrie : public Trie<ScopedStmtTrie> {
  public:
    /// @enum `Pattern`
    /// @brief An enum of all possible patterns a scoped statement could have
    enum class Pattern {
        IF,
        FOR,
        ENH_FOR,
        WHILE,
        DO_WHILE,
        CATCH,
        SWITCH,
    };

    /// @function `init`
    /// @brief Seeds the root node of the scoped statement trie with its full pattern range (every pattern from the first to the last
    /// enumerator, inclusive)
    static void init(Trie<ScopedStmtTrie>::RootNode<Pattern> &root) {
        Trie<ScopedStmtTrie>::RootNode<Pattern>::init(root, Pattern::IF, Pattern::SWITCH);
    }

    /// @var `matchers`
    /// @brief A map which maps all possible patterns to their matching functions and their trie affinity
    static inline const Trie<ScopedStmtTrie>::matchers_map<Pattern> matchers = {
        {
            Pattern::IF,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_match(tokens, Matcher::if_statement)      //
                        || Matcher::tokens_match(tokens, Matcher::else_if_statement) //
                        || Matcher::tokens_match(tokens, Matcher::else_statement);
                },
            },
        },
        {
            Pattern::FOR,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::for_loop); },
            },
        },
        {
            Pattern::ENH_FOR,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::enhanced_for_loop); },
            },
        },
        {
            Pattern::WHILE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::while_loop); },
            },
        },
        {
            Pattern::DO_WHILE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::do_while_loop); },
            },
        },
        {
            Pattern::CATCH,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) { return Matcher::tokens_end_with(tokens, Matcher::catch_statement); },
            },
        },
        {
            Pattern::SWITCH,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) { return Matcher::tokens_end_with(tokens, Matcher::switch_statement); },
            },
        },
    };
};
