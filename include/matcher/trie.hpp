#include "matcher.hpp"

#include <functional>

class Trie {
  public:
    Trie() = delete;

    using verify_fn = std::function<bool(const token_slice &)>;

    class Stmt {
      public:
        Stmt() = delete;

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
            RETURN_STATEMENT,
            THROW_STATEMENT,
            ALIASED_FUNCTION_CALL,
            FUNCTION_CALL,
            UNARY_OP_STATEMENT,
            BREAK_STATEMENT,
            CONTINUE_STATEMENT,
        };

        static const inline std::vector<std::pair<Pattern, verify_fn>> matchers = {
            {
                Pattern::GROUP_DECLARATION_INFERRED,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::group_declaration_inferred); },
            },
            {
                Pattern::DECLARATION_EXPLICIT,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::declaration_explicit); },
            },
            {
                Pattern::DECLARATION_INFERRED,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::declaration_inferred); },
            },
            {
                Pattern::DECLARATION_WITHOUT_INITIALIZER,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::declaration_without_initializer); },
            },
            {
                Pattern::DATA_FIELD_ASSIGNMENT,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::data_field_assignment); },
            },
            {
                Pattern::DATA_FIELD_ASSIGNMENT_SHORTHAND,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::data_field_assignment_shorthand); },
            },
            {
                Pattern::GROUPED_DATA_ASSIGNMENT,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::grouped_data_assignment); },
            },
            {
                Pattern::GROUPED_DATA_ASSIGNMENT_SHORTHAND,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::grouped_data_assignment_shorthand); },
            },
            {
                Pattern::GROUP_ASSIGNMENT,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::group_assignment); },
            },
            {
                Pattern::GROUP_ASSIGNMENT_SHORTHAND,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::group_assignment_shorthand); },
            },
            {
                Pattern::ARRAY_ASSIGNMENT,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::array_assignment); },
            },
            {
                Pattern::ARRAY_ASSIGNMENT_SHORTHAND,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::array_assignment_shorthand); },
            },
            {
                Pattern::GROUPED_ARRAY_ASSIGNMENT,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::grouped_array_assignment); },
            },
            {
                Pattern::GROUPED_ARRAY_ASSIGNMENT_SHORTHAND,
                [](const token_slice &tokens) { return Matcher::tokens_contain(tokens, Matcher::grouped_array_assignment_shorthand); },
            },
            {
                Pattern::ASSIGNMENT,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::assignment); },
            },
            {
                Pattern::ASSIGNMENT_SHORTHAND,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::assignment_shorthand); },
            },
            {
                Pattern::DISCARD_ASSIGNMENT,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::discard_assignment); },
            },
            {
                Pattern::RETURN_STATEMENT,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::return_statement); },
            },
            {
                Pattern::THROW_STATEMENT,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::throw_statement); },
            },
            {
                Pattern::ALIASED_FUNCTION_CALL,
                [](const token_slice &tokens) { return Matcher::tokens_start_with(tokens, Matcher::aliased_function_call); },
            },
            {
                Pattern::FUNCTION_CALL,
                [](const token_slice &tokens) {
                    return Matcher::tokens_start_with(tokens, Matcher::function_call)  //
                        || Matcher::tokens_start_with(tokens, Matcher::instance_call); //
                },
            },
            {
                Pattern::UNARY_OP_STATEMENT,
                [](const token_slice &tokens) {
                    return Matcher::tokens_end_with_continuous(                                                                          //
                        token_slice{tokens.first, std::prev(tokens.second)}, Matcher::unary_post_operator, Matcher::expression_separator //
                    );
                },
            },
            {
                Pattern::BREAK_STATEMENT,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::break_statement); },
            },
            {
                Pattern::CONTINUE_STATEMENT,
                [](const token_slice &tokens) { return Matcher::tokens_match(tokens, Matcher::continue_statement); },
            },
        };

        static std::optional<Pattern> match(const token_slice &tokens) {
            for (const auto &[pattern, fn] : matchers) {
                if (fn(tokens)) {
                    return pattern;
                }
            }
            return std::nullopt;
        }
    };
};
