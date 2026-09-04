#pragma once

#include "matcher.hpp"
#include "trie.hpp"

/// @class `StmtTrie`
/// @brief This class is an implementation of the trie to match all statements
class StmtTrie : public Trie<StmtTrie> {
  public:
    /// @enum `Pattern`
    /// @brief An enum of all possible patterns a statement could have
    enum class Pattern {
        GROUP_DECLARATION_INFERRED,
        DECLARATION_EXPLICIT,
        DECLARATION_INFERRED,
        DECLARATION_WITHOUT_INITIALIZER,
        DATA_FIELD_ASSIGNMENT,
        DATA_FIELD_ASSIGNMENT_SHORTHAND,
        GROUPED_DATA_ASSIGNMENT,
        GROUPED_DATA_ASSIGNMENT_SHORTHAND,
        GROUP_ASSIGNMENT,
        GROUP_ASSIGNMENT_SHORTHAND,
        ARRAY_ASSIGNMENT,
        ARRAY_ASSIGNMENT_SHORTHAND,
        GROUPED_ARRAY_ASSIGNMENT,
        GROUPED_ARRAY_ASSIGNMENT_SHORTHAND,
        ASSIGNMENT,
        ASSIGNMENT_SHORTHAND,
        DISCARD_ASSIGNMENT,
        RETURN,
        THROW,
        ALIASED_FUNCTION_CALL,
        FUNCTION_CALL,
        UNARY_OP,
        BREAK,
        CONTINUE,
    };

    /// @function `init`
    /// @brief Seeds the root node of the statement trie with its full pattern range (every pattern from the first to the last
    /// enumerator, inclusive)
    static void init(Trie<StmtTrie>::RootNode<Pattern> &root) {
        Trie<StmtTrie>::RootNode<Pattern>::init(root, Pattern::GROUP_DECLARATION_INFERRED, Pattern::CONTINUE);
    }

    /// @var `matchers`
    /// @brief A map which maps all possible patterns to their matching functions and their trie affinity
    static inline const Trie<StmtTrie>::matchers_map<Pattern> matchers = {
        {
            Pattern::GROUP_DECLARATION_INFERRED,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::group_declaration_inferred); },
            },
        },
        {
            Pattern::DECLARATION_EXPLICIT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::declaration_explicit); },
            },
        },
        {
            Pattern::DECLARATION_INFERRED,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::declaration_inferred); },
            },
        },
        {
            Pattern::DECLARATION_WITHOUT_INITIALIZER,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::declaration_without_initializer); },
            },
        },
        {
            Pattern::DATA_FIELD_ASSIGNMENT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_contain_at_top_level(tokens, Matcher::data_field_assignment); },
            },
        },
        {
            Pattern::DATA_FIELD_ASSIGNMENT_SHORTHAND,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_contain_at_top_level(tokens, Matcher::data_field_assignment_shorthand);
                },
            },
        },
        {
            Pattern::GROUPED_DATA_ASSIGNMENT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_contain_at_top_level(tokens, Matcher::grouped_data_assignment); },
            },
        },
        {
            Pattern::GROUPED_DATA_ASSIGNMENT_SHORTHAND,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_contain_at_top_level(tokens, Matcher::grouped_data_assignment_shorthand);
                },
            },
        },
        {
            Pattern::GROUP_ASSIGNMENT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::group_assignment); },
            },
        },
        {
            Pattern::GROUP_ASSIGNMENT_SHORTHAND,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_contain_at_top_level(tokens, Matcher::group_assignment_shorthand); },
            },
        },
        {
            Pattern::ARRAY_ASSIGNMENT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_contain_at_top_level(tokens, Matcher::array_assignment); },
            },
        },
        {
            Pattern::ARRAY_ASSIGNMENT_SHORTHAND,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_contain_at_top_level(tokens, Matcher::array_assignment_shorthand); },
            },
        },
        {
            Pattern::GROUPED_ARRAY_ASSIGNMENT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_contain_at_top_level(tokens, Matcher::grouped_array_assignment); },
            },
        },
        {
            Pattern::GROUPED_ARRAY_ASSIGNMENT_SHORTHAND,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_contain_at_top_level(tokens, Matcher::grouped_array_assignment_shorthand);
                },
            },
        },
        {
            Pattern::ASSIGNMENT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::assignment); },
            },
        },
        {
            Pattern::ASSIGNMENT_SHORTHAND,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::assignment_shorthand); },
            },
        },
        {
            Pattern::DISCARD_ASSIGNMENT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::discard_assignment); },
            },
        },
        {
            Pattern::RETURN,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::return_statement); },
            },
        },
        {
            Pattern::THROW,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::throw_statement); },
            },
        },
        {
            Pattern::ALIASED_FUNCTION_CALL,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::aliased_function_call); },
            },
        },
        {
            Pattern::FUNCTION_CALL,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_start_with(tokens, Matcher::function_call)  //
                        || Matcher::tokens_start_with(tokens, Matcher::instance_call); //
                },
            },
        },
        {
            Pattern::UNARY_OP,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_end_with_continuous(                                                                          //
                        token_slice{tokens.first, std::prev(tokens.second)}, Matcher::unary_post_operator, Matcher::expression_separator //
                    );
                },
            },
        },
        {
            Pattern::BREAK,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::break_statement); },
            },
        },
        {
            Pattern::CONTINUE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::continue_statement); },
            },
        },
    };
};
