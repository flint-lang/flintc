#include "parser.hpp"

#include "../types.hpp"
#include "../lexer/lexer.hpp"
#include "../error/error.hpp"
#include "ast/definitions/data_node.hpp"
#include "ast/definitions/entity_node.hpp"
#include "ast/definitions/enum_node.hpp"
#include "ast/definitions/error_node.hpp"
#include "ast/definitions/func_node.hpp"
#include "ast/definitions/function_node.hpp"
#include "ast/definitions/import_node.hpp"
#include "ast/definitions/variant_node.hpp"
#include "ast/node_type.hpp"
#include "ast/program_node.hpp"

#include "../debug.hpp"
#include "signature.hpp"

#include <algorithm>
#include <vector>
#include <string>
#include <iterator>

/// parse_file
///     Parses a file. It will tokenize it using the Lexer and then create the AST of the file and add all the nodes to the passed main ProgramNode
using Signature::get_line_token_indices;
using Signature::tokens_contain;

void Parser::parse_file(ProgramNode &program, std::string &file)
{
    token_list tokens = Lexer(file).scan();
    // Consume all tokens and convert them to nodes
    while(!tokens.empty()) {
        add_next_main_node(program, tokens);
    }
}

/// get_next_main_node
///     Finds the next main node inside the list of tokens and creates an ASTNode "tree" from it.
///     It returns the built ASTNode tree
void Parser::add_next_main_node(ProgramNode &program, token_list &tokens) {
    token_list definition_tokens = get_definition_tokens(tokens);
    NodeType next_type = what_is_this(definition_tokens);

    // Find the indentation of the definition
    int definition_indentation = 0;
    for(const TokenContext &tok : definition_tokens) {
        if(tok.type == TOK_INDENT) {
            definition_indentation++;
        } else {
            break;
        }
    }

    switch(next_type) {
        case NodeType::NONE: {
            throw_err(ERR_UNEXPECTED_DEFINITION);
            break;
        }
        case NodeType::IMPORT: {
            if(definition_indentation > 0) {
                throw_err(ERR_USE_STATEMENT_MUST_BE_AT_TOP_LEVEL);
            }
            ImportNode import_node = ImportNode();
            program.add_import(import_node);
            break;
        }
        case NodeType::FUNCTION: {
            token_list body_tokens = get_body_tokens(definition_indentation, tokens);
            FunctionNode function_node = create_function(definition_tokens, body_tokens);
            program.add_function(function_node);
            break;
        }
        case NodeType::DATA: {
            token_list body_tokens = get_body_tokens(definition_indentation, tokens);
            DataNode data_node = create_data(definition_tokens, body_tokens);
            program.add_data(data_node);
            break;
        }
        case NodeType::FUNC: {
            token_list body_tokens = get_body_tokens(definition_indentation, tokens);
            FuncNode func_node = create_func(definition_tokens, body_tokens);
            program.add_func(func_node);
            break;
        }
        case NodeType::ENTITY: {
            token_list body_tokens = get_body_tokens(definition_indentation, tokens);
            EntityNode entity_node = create_entity(definition_tokens, body_tokens);
            program.add_entity(entity_node);
            break;
        }
        case NodeType::ENUM: {
            token_list body_tokens = get_body_tokens(definition_indentation, tokens);
            EnumNode enum_node = create_enum(definition_tokens, body_tokens);
            program.add_enum(enum_node);
            break;
        }
        case NodeType::ERROR: {
            token_list body_tokens = get_body_tokens(definition_indentation, tokens);
            ErrorNode error_node = create_error(definition_tokens, body_tokens);
            program.add_error(error_node);
            break;
        }
        case NodeType::VARIANT: {
            token_list body_tokens = get_body_tokens(definition_indentation, tokens);
            VariantNode variant_node = create_variant(definition_tokens, body_tokens);
            program.add_variant(variant_node);
            break;
        }
    }
}

/// what_is_this
///     Gets a list of tokens (from one line), checks its signature and returns the type of the node which matches this signature.
NodeType Parser::what_is_this(const token_list &tokens) {
    Debug::print_token_context_vector(tokens);

    if (Signature::tokens_contain(tokens, Signature::function_definition)) {
        return NodeType::FUNCTION;
    }
    if (Signature::tokens_contain(tokens, Signature::data_definition)) {
        return NodeType::DATA;
    }
    if (Signature::tokens_contain(tokens, Signature::func_definition)) {
        return NodeType::FUNC;
    }
    if (Signature::tokens_contain(tokens, Signature::entity_definition)) {
        return NodeType::ENTITY;
    }
    if (Signature::tokens_contain(tokens, Signature::enum_definition)) {
        return NodeType::ENUM;
    }
    if (Signature::tokens_contain(tokens, Signature::error_definition)) {
        return NodeType::ERROR;
    }
    if (Signature::tokens_contain(tokens, Signature::variant_definition)) {
        return NodeType::VARIANT;
    }
    if (Signature::tokens_contain(tokens, Signature::use_statement)) {
        return NodeType::IMPORT;
    }
    return NodeType::NONE;
}

/// get_definition_tokens
///     Returns all the tokens which are part of the definition. Removes all other tokens from the token vector
token_list Parser::get_definition_tokens(token_list &tokens) {
    // Scan through all the tokens and first extract all tokens from this line
    int end_index = 0;
    int start_line = tokens.at(0).line;
    for(const TokenContext &tok : tokens) {
        if(tok.line == start_line) {
            end_index ++;
        } else {
            break;
        }
    }
    // Move elements from the tokens vector to a new slice vector and remove all these elements from the tokens vector
    token_list definition_tokens;
    definition_tokens.reserve(end_index);
    std::cout << "tokens size: " << tokens.size() << "\n";
    std::cout << "end_index: " << end_index << "\n";
    std::cout << "definition size: " << definition_tokens.size() << "\n";
    std::move(tokens.begin(), tokens.begin() + end_index, std::back_inserter(definition_tokens));
    tokens.erase(tokens.begin(), tokens.begin() + end_index);
    return definition_tokens;
}

/// get_body_tokens
///     Extracts all the body tokens based on their indentation
token_list Parser::get_body_tokens(unsigned int definition_indentation, token_list &tokens) {
    int current_line = tokens.at(0).line;
    int end_idx = 0;
    for(const TokenContext &tok : tokens) {
        if(tok.line != current_line) {
            current_line = tok.line;
            std::optional<unsigned int> indents_maybe = Signature::get_leading_indents(tokens, current_line);
            if(indents_maybe.has_value() && indents_maybe.value() <= definition_indentation) {
                break;
            }
        }
        end_idx++;
    }
    if(end_idx == 0) {
        throw_err(ERR_NO_BODY_DECLARED);
    }

    token_list body;
    body.reserve(end_idx);
    std::move(tokens.begin(), tokens.begin() + end_idx, std::back_inserter(body));
    tokens.erase(tokens.begin(), tokens.begin() + end_idx);

    return body;
}

/// create_function
///
FunctionNode Parser::create_function(const token_list &definition, const token_list &body) {
    return {};
}

/// create_data
///
DataNode Parser::create_data(const token_list &definition, const token_list &body) {
    return {};
}

/// create_func
///
FuncNode Parser::create_func(const token_list &definition, const token_list &body) {
    return {};
}

/// create_entity
///
EntityNode Parser::create_entity(const token_list &definition, const token_list &body) {
    return {};
}

/// create_enum
///
EnumNode Parser::create_enum(const token_list &definition, const token_list &body) {
    return {};
}

/// create_error
///
ErrorNode Parser::create_error(const token_list &definition, const token_list &body) {
    return {};
}

/// create_variant
///
VariantNode Parser::create_variant(const token_list &definition, const token_list &body) {
    return {};
}
