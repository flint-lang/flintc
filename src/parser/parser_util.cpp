#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "parser/signature.hpp"

void Parser::add_next_main_node(FileNode &file_node, token_list &tokens) {
    token_list definition_tokens = get_definition_tokens(tokens);

    // Find the indentation of the definition
    int definition_indentation = 0;
    for (const TokenContext &tok : definition_tokens) {
        if (tok.type == TOK_INDENT) {
            definition_indentation++;
        } else {
            break;
        }
    }

    if (Signature::tokens_contain(definition_tokens, Signature::use_statement)) {
        if (definition_indentation > 0) {
            THROW_ERR(ErrUseStatementNotAtTopLevel, ERR_PARSING, file_name, definition_tokens);
            std::exit(EXIT_FAILURE);
        }
        ImportNode import_node = create_import(definition_tokens);
        file_node.add_import(import_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::function_definition)) {
        token_list body_tokens = get_body_tokens(definition_indentation, tokens);
        std::optional<FunctionNode> function_node = create_function(definition_tokens, body_tokens);
        if (!function_node.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            std::exit(EXIT_FAILURE);
        }
        file_node.add_function(function_node.value());
    } else if (Signature::tokens_contain(definition_tokens, Signature::data_definition)) {
        token_list body_tokens = get_body_tokens(definition_indentation, tokens);
        DataNode data_node = create_data(definition_tokens, body_tokens);
        file_node.add_data(data_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::func_definition)) {
        token_list body_tokens = get_body_tokens(definition_indentation, tokens);
        FuncNode func_node = create_func(definition_tokens, body_tokens);
        file_node.add_func(func_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::entity_definition)) {
        token_list body_tokens = get_body_tokens(definition_indentation, tokens);
        create_entity_type entity_creation = create_entity(definition_tokens, body_tokens);
        file_node.add_entity(entity_creation.first);
        if (entity_creation.second.has_value()) {
            std::unique_ptr<DataNode> data_node_ptr = std::move(entity_creation.second.value().first);
            std::unique_ptr<FuncNode> func_node_ptr = std::move(entity_creation.second.value().second);
            file_node.add_data(*data_node_ptr);
            file_node.add_func(*func_node_ptr);
        }
    } else if (Signature::tokens_contain(definition_tokens, Signature::enum_definition)) {
        token_list body_tokens = get_body_tokens(definition_indentation, tokens);
        EnumNode enum_node = create_enum(definition_tokens, body_tokens);
        file_node.add_enum(enum_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::error_definition)) {
        token_list body_tokens = get_body_tokens(definition_indentation, tokens);
        ErrorNode error_node = create_error(definition_tokens, body_tokens);
        file_node.add_error(error_node);
    } else if (Signature::tokens_contain(definition_tokens, Signature::variant_definition)) {
        token_list body_tokens = get_body_tokens(definition_indentation, tokens);
        VariantNode variant_node = create_variant(definition_tokens, body_tokens);
        file_node.add_variant(variant_node);
    } else {
        Debug::print_token_context_vector(definition_tokens);
        THROW_ERR(ErrUnexpectedDefinition, ERR_PARSING, file_name, definition_tokens);
        std::exit(EXIT_FAILURE);
    }
}

token_list Parser::get_definition_tokens(token_list &tokens) {
    // Scan through all the tokens and first extract all tokens from this line
    int end_index = 0;
    int start_line = tokens.at(0).line;
    for (const TokenContext &tok : tokens) {
        if (tok.line == start_line) {
            end_index++;
        } else {
            break;
        }
    }
    return extract_from_to(0, end_index, tokens);
}

token_list Parser::get_body_tokens(unsigned int definition_indentation, token_list &tokens) {
    int current_line = tokens.at(0).line;
    int end_idx = 0;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it->line != current_line || it == tokens.begin()) {
            current_line = it->line;
            std::optional<unsigned int> indents_maybe = Signature::get_leading_indents(tokens, current_line);
            if (indents_maybe.has_value() && indents_maybe.value() <= definition_indentation) {
                break;
            }
        }
        end_idx++;
    }
    if (end_idx == 0) {
        THROW_ERR(ErrMissingBody, ERR_PARSING, file_name, tokens);
        std::exit(EXIT_FAILURE);
    }

    return extract_from_to(0, end_idx, tokens);
}

token_list Parser::extract_from_to(unsigned int from, unsigned int to, token_list &tokens) {
    token_list extraction = clone_from_to(from, to, tokens);
    tokens.erase(tokens.begin() + from, tokens.begin() + to);
    return extraction;
}

token_list Parser::clone_from_to(unsigned int from, unsigned int to, const token_list &tokens) {
    assert(to >= from);
    assert(to <= tokens.size());
    token_list extraction;
    if (to == from) {
        return extraction;
    }
    extraction.reserve(to - from);
    std::copy(tokens.begin() + from, tokens.begin() + to, std::back_inserter(extraction));
    return extraction;
}

std::optional<std::tuple<std::string, std::vector<std::unique_ptr<ExpressionNode>>, std::string>> Parser::create_call_base( //
    Scope *scope,                                                                                                           //
    token_list &tokens                                                                                                      //
) {
    std::optional<uint2> arg_range = Signature::balanced_range_extraction(tokens, {{TOK_LEFT_PAREN}}, {{TOK_RIGHT_PAREN}});
    if (!arg_range.has_value()) {
        return std::nullopt;
    }
    // remove the '(' and ')' tokens from the arg_range
    ++arg_range.value().first;
    --arg_range.value().second;

    std::string function_name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;

    for (const auto &tok : tokens) {
        // Get the function name
        if (tok.type == TOK_IDENTIFIER) {
            function_name = tok.lexme;
            break;
        }
    }

    // Arguments are separated by commas. When the arg_range.first == arg_range.second, no arguments are passed
    if (arg_range.value().first < arg_range.value().second) {
        // if the args contain at least one comma, it is known that multiple arguments are passed. If not, only one is
        // passed
        if (Signature::tokens_contain_in_range(tokens, {{TOK_COMMA}}, arg_range.value())) {
            const auto match_ranges = Signature::get_match_ranges_in_range(tokens, {{TOK_COMMA}}, arg_range.value());
            if (match_ranges.empty()) {
                // No arguments
                return std::make_tuple(function_name, std::move(arguments), "");
            }

            for (auto match = match_ranges.begin();; ++match) {
                token_list argument_tokens;
                if (match == match_ranges.begin()) {
                    argument_tokens = clone_from_to(arg_range.value().first, match->first, tokens);
                } else if (match == match_ranges.end()) {
                    argument_tokens = clone_from_to((match - 1)->second, arg_range.value().second, tokens);
                } else {
                    argument_tokens = clone_from_to((match - 1)->second, match->first, tokens);
                }
                auto expression = create_expression(scope, argument_tokens);
                if (!expression.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, argument_tokens);
                    return std::nullopt;
                }
                arguments.emplace_back(std::move(expression.value()));
                if (match == match_ranges.end()) {
                    break;
                }
            }
        } else {
            token_list argument_tokens = extract_from_to(arg_range.value().first, arg_range.value().second, tokens);
            auto expression = create_expression(scope, argument_tokens);
            if (!expression.has_value()) {
                THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, argument_tokens);
                return std::nullopt;
            }
            arguments.emplace_back(std::move(expression.value()));
        }
    }

    return std::make_tuple(function_name, std::move(arguments), "");
}
