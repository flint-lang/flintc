#pragma once

#include "lexer/token.hpp"
#include "matcher.hpp"
#include "trie.hpp"

/// @class `ExprTrie`
/// @brief This class is an implementation of the trie to match all expressions. Expressions are usually only recognizable by their
/// trailing tokens, so the backward side of the trie walks their suffixes most deeply
class ExprTrie : public Trie<ExprTrie, 4> {
  public:
    /// @enum `Pattern`
    /// @brief An enum of all possible patterns an expression could have
    enum class Pattern {
        LITERAL,
        VARIABLE,
        DEFAULT,
        TYPE,
        RANGE,
        LITERAL_EXPR,
        STRING_INTERPOLATION,
        ALIASED_FUNCTION_CALL,
        FUNCTION_CALL,
        GROUP,
        INITIALIZER,
        TYPE_CAST,
        ANONYMOUS_ERROR,
        UNARY_OP,
        TYPE_FIELD_ACCESS,
        FUNCTION_REFERENCE,
        OPTIONAL_CHAIN,
        DATA_ACCESS,
        GROUPED_DATA_ACCESS,
        ARRAY_INITIALIZER,
        ARRAY_ACCESS,
        GROUPED_ARRAY_ACCESS,
        OPTIONAL_UNWRAP,
        VARIANT_EXTRACTION,
        VARIANT_UNWRAP,
        RANGE_EXPRESSION,
        BINARY_OP,
    };

    /// @function `init`
    /// @brief Seeds the root node of the expression trie with its full pattern range (every pattern from the first to the last
    /// enumerator, inclusive)
    static void init(Trie<ExprTrie, 4>::RootNode<Pattern> &root) {
        Trie<ExprTrie, 4>::RootNode<Pattern>::init(root, Pattern::LITERAL, Pattern::BINARY_OP);
    }

    /// @var `matchers`
    /// @brief A map which maps all possible patterns to their matching functions and their trie affinity
    static inline const Trie<ExprTrie, 4>::matchers_map<Pattern> matchers = {
        {
            Pattern::LITERAL,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    return token_size == 1 && Matcher::tokens_match(tokens, Matcher::literal);
                },
            },
        },
        {
            Pattern::VARIABLE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    return token_size == 1 && Matcher::tokens_match(tokens, Matcher::variable_expr);
                },
            },
        },
        {
            Pattern::DEFAULT,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    return token_size == 1 && Matcher::tokens_match(tokens, Matcher::token(TOK_UNDERSCORE));
                },
            },
        },
        {
            Pattern::TYPE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    return token_size == 1 && Matcher::tokens_match(tokens, Matcher::token(TOK_TYPE));
                },
            },
        },
        {
            Pattern::RANGE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    return token_size == 1 && Matcher::tokens_match(tokens, Matcher::token(TOK_RANGE));
                },
            },
        },
        {
            Pattern::LITERAL_EXPR,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    return token_size == 2 && Matcher::tokens_match(tokens, Matcher::literal_expr);
                },
            },
        },
        {
            Pattern::STRING_INTERPOLATION,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    return token_size == 2 && Matcher::tokens_match(tokens, Matcher::string_interpolation);
                },
            },
        },
        {
            Pattern::ALIASED_FUNCTION_CALL,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    // Its only a call, when the paren group of the function is at the very end of the tokens, otherwise there is
                    // something located on the right of the call still
                    const auto range = Matcher::balanced_range_extraction(                      //
                        tokens, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
                    );
                    return range.has_value()                                              //
                        && range.value().second == token_size                             //
                        && Matcher::tokens_match(tokens, Matcher::aliased_function_call); //
                },
            },
        },
        {
            Pattern::FUNCTION_CALL,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    // Its only a call, when the paren group of the function is at the very end of the tokens, otherwise there is
                    // something located on the right of the call still
                    const auto range = Matcher::balanced_range_extraction(                      //
                        tokens, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
                    );
                    return range.has_value()                  //
                        && range.value().second == token_size //
                        &&
                        (Matcher::tokens_match(tokens, Matcher::function_call) || Matcher::tokens_match(tokens, Matcher::instance_call)); //
                },
            },
        },
        {
            Pattern::GROUP,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    const auto range = Matcher::balanced_range_extraction(                      //
                        tokens, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
                    );
                    return range.has_value()                                         //
                        && range.value().first == 0                                  //
                        && range.value().second == token_size                        //
                        && Matcher::tokens_match(tokens, Matcher::group_expression); //
                },
            },
        },
        {
            Pattern::INITIALIZER,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    if (token_size < 3) {
                        return false;
                    }
                    return Matcher::tokens_match(tokens, Matcher::initializer);
                },
            },
        },
        {
            Pattern::TYPE_CAST,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    if (token_size < 3) {
                        return false;
                    }
                    if (!Matcher::tokens_match(tokens, Matcher::type_cast)) {
                        return false;
                    }
                    ASSERT(tokens.first->token == TOK_TYPE);
                    const Type::Variation variation = tokens.first->type->get_variation();
                    return variation == Type::Variation::PRIMITIVE || variation == Type::Variation::VECTOR;
                },
            },
        },
        {
            Pattern::ANONYMOUS_ERROR,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::anonymous_error); },
            },
        },
        {
            Pattern::UNARY_OP,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    const bool is_literal_expr = token_size == 2 && Matcher::tokens_match(tokens, Matcher::literal_expr);
                    return !is_literal_expr                                                                                           //
                        && (Matcher::tokens_start_with_continuous(tokens, Matcher::unary_pre_operator, Matcher::expression_separator) //
                               || Matcher::tokens_end_with_continuous(tokens, Matcher::unary_post_operator, Matcher::expression_separator));
                },
            },
        },
        {
            Pattern::TYPE_FIELD_ACCESS,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    if (token_size != 3 && !(token_size == 4 && std::prev(tokens.second)->token == TOK_INT_VALUE)) {
                        return false;
                    }
                    if (tokens.first->token != TOK_TYPE) {
                        return false;
                    }
                    switch (tokens.first->type->get_variation()) {
                        default:
                            return false;
                        case Type::Variation::DATA: {
                            const DataNode *data_node = tokens.first->type->as<DataType>()->data_node;
                            if (!data_node->is_const && !data_node->is_shared) {
                                return false;
                            }
                            break;
                        }
                        case Type::Variation::ENUM:
                            break;
                        case Type::Variation::ERROR_SET:
                            break;
                        case Type::Variation::VARIANT:
                            break;
                    }
                    return Matcher::tokens_match(tokens, Matcher::type_field_access);
                },
            },
        },
        {
            Pattern::FUNCTION_REFERENCE,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    return (token_size == 2 || token_size == 3) && Matcher::tokens_match(tokens, Matcher::function_reference);
                },
            },
        },
        {
            Pattern::OPTIONAL_CHAIN,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_end_with_continuous(tokens, Matcher::optional_chain, Matcher::expression_separator);
                },
            },
        },
        {
            Pattern::DATA_ACCESS,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_end_with_continuous(tokens, Matcher::data_access, Matcher::expression_separator);
                },
            },
        },
        {
            Pattern::GROUPED_DATA_ACCESS,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_end_with_continuous(tokens, Matcher::grouped_data_access, Matcher::expression_separator);
                },
            },
        },
        {
            Pattern::ARRAY_INITIALIZER,
            {
                TrieAffinity::FORWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    if (token_size < 3) {
                        return false;
                    }
                    const bool is_array = tokens.first->token == TOK_TYPE && tokens.first->type->get_variation() == Type::Variation::ARRAY;
                    const bool brackets_follow_type = std::next(tokens.first)->token == TOK_LEFT_BRACKET;
                    return (is_array || brackets_follow_type) && Matcher::tokens_match(tokens, Matcher::array_initializer);
                },
            },
        },
        {
            Pattern::ARRAY_ACCESS,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    if (std::prev(tokens.second)->token != TOK_RIGHT_BRACKET) {
                        return false;
                    }
                    return Matcher::tokens_end_with_continuous(tokens, Matcher::array_access, Matcher::expression_separator);
                },
            },
        },
        {
            Pattern::GROUPED_ARRAY_ACCESS,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    if (std::prev(tokens.second)->token != TOK_RIGHT_BRACKET) {
                        return false;
                    }
                    return Matcher::tokens_end_with_continuous(tokens, Matcher::grouped_array_access, Matcher::expression_separator);
                },
            },
        },
        {
            Pattern::OPTIONAL_UNWRAP,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_end_with_continuous(tokens, Matcher::optional_unwrap, Matcher::expression_separator);
                },
            },
        },
        {
            Pattern::VARIANT_EXTRACTION,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_end_with_continuous(tokens, Matcher::variant_extraction, Matcher::expression_separator);
                },
            },
        },
        {
            Pattern::VARIANT_UNWRAP,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    return Matcher::tokens_end_with_continuous(tokens, Matcher::variant_unwrap, Matcher::expression_separator);
                },
            },
        },
        {
            Pattern::RANGE_EXPRESSION,
            {
                TrieAffinity::BACKWARD,
                [](const token_slice &tokens) {
                    const size_t token_size = std::distance(tokens.first, tokens.second);
                    const std::vector<uint2> range_expr_matches = Matcher::get_match_ranges_in_range_outside_group( //
                        tokens,                                                                                     //
                        Matcher::range_expression,                                                                  //
                        {0, token_size},                                                                            //
                        Matcher::token(TOK_LEFT_BRACKET),                                                           //
                        Matcher::token(TOK_RIGHT_BRACKET)                                                           //
                    );
                    return range_expr_matches.size() == 1;
                },
            },
        },
        {
            Pattern::BINARY_OP,
            {
                TrieAffinity::FORWARD,
                []([[maybe_unused]] const token_slice &tokens) {
                    // Should never be matched be unreachable from within here.
                    return false;
                },
            },
        },
    };
};
