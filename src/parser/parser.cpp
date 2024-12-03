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
#include "ast/statements/statement_node.hpp"
#include "signature.hpp"

#include <algorithm>
#include <cassert>
#include <vector>
#include <string>
#include <iterator>

/// parse_file
///     Parses a file. It will tokenize it using the Lexer and then create the AST of the file and add all the nodes to the passed main ProgramNode
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
///     Only Definition nodes are considered as 'main' nodes
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
///     Returns all the tokens which are part of the definition.
///     Deletes all tokens which are part of the definition from the given tokens list
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
    return extract_from_to(0, end_index, tokens);
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

    return extract_from_to(0, end_idx, tokens);
}

/// extract_from_to
///     Extracts the tokens from a given index up to the given index from the given tokens list
///     Extracts [from ; to) tokens
///     Also deletes all the extracted tokens from the token list
token_list Parser::extract_from_to(unsigned int from, unsigned int to, token_list &tokens) {
    assert(to > from);
    token_list extraction;
    extraction.reserve(to - from);
    std::move(tokens.begin() + from, tokens.begin() + to, std::back_inserter(extraction));
    tokens.erase(tokens.begin() + from, tokens.begin() + to);
    return extraction;
}

/// create_body
///     Creates a body containing of multiple statement nodes from a list of tokens
std::vector<StatementNode> Parser::create_body(const token_list &body) {
    std::vector<StatementNode> body_statements;

    return body_statements;
}

/// create_function
///     Creates a FunctionNode from the given definiton tokens of the FunctionNode as well as its body. Will cause additional creation of AST Nodes for the body
FunctionNode Parser::create_function(const token_list &definition, const token_list &body) {
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters;
    std::vector<std::string> return_types;
    bool is_aligned = false;
    bool is_const = false;

    bool begin_params = false;
    bool begin_returns = false;
    auto tok_iterator = definition.begin();
    while(tok_iterator != definition.end()) {
        if(tok_iterator->type == TOK_ALIGNED) {
            is_aligned = true;
        }
        if(tok_iterator->type == TOK_CONST && name.empty()) {
            is_const = true;
        }
        // Finding the function name
        if(tok_iterator->type == TOK_DEF) {
            name = (tok_iterator + 1)->lexme;
        }
        // Adding the functions parameters
        if(tok_iterator->type == TOK_LEFT_PAREN && !begin_returns) {
            begin_params = true;
        }
        if (tok_iterator->type == TOK_RIGHT_PAREN && begin_params) {
            begin_params = false;
        }
        if(begin_params && Signature::tokens_match({TokenContext {tok_iterator->type, "", 0}}, Signature::type)
            && (tok_iterator + 1)->type == TOK_IDENTIFIER)
        {
            parameters.emplace_back(tok_iterator->lexme, (tok_iterator + 1)->lexme);
        }
        // Adding the functions return types
        if(tok_iterator->type == TOK_ARROW) {
            // Only one return type
            if((tok_iterator + 1)->type == TOK_IDENTIFIER) {
                return_types.push_back((tok_iterator + 1)->lexme);
                break;
            }
            begin_returns = true;;
        }
        if(begin_returns && tok_iterator->type == TOK_IDENTIFIER) {
            return_types.push_back(tok_iterator->lexme);
        }
        if(begin_returns && tok_iterator->type == TOK_RIGHT_PAREN) {
            break;
        }
        ++tok_iterator;
    }
    std::vector<StatementNode> body_node = create_body(body);
    FunctionNode function(is_aligned, is_const, name, parameters, return_types, body_node);
    return function;
}

/// create_data
///     Creates a DataNode from the given definition and body tokens.
DataNode Parser::create_data(const token_list &definition, const token_list &body) {
    bool is_shared = false;
    bool is_immutable = false;
    bool is_aligned = false;
    std::string name;

    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<std::pair<std::string, std::string>> default_values;
    std::vector<std::string> order;

    auto definition_iterator = definition.begin();
    while (definition_iterator != definition.end()) {
        if(definition_iterator->type == TOK_SHARED) {
            is_shared = true;
        }
        if(definition_iterator->type == TOK_IMMUTABLE) {
            is_immutable = true;
            // immutable data is shared by default
            is_shared = true;
        }
        if(definition_iterator->type == TOK_ALIGNED) {
            is_aligned = true;
        }
        if(definition_iterator->type == TOK_DATA) {
            name = (definition_iterator + 1)->lexme;
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    bool parsing_constructor = false;
    while (body_iterator != body.end()) {
        if(Signature::tokens_match({TokenContext{body_iterator->type, "", 0}}, Signature::type) && (body_iterator + 1)->type == TOK_IDENTIFIER) {
            fields.emplace_back(
                body_iterator->lexme,
                (body_iterator + 1)->lexme
            );
            if((body_iterator + 2)->type == TOK_EQUAL) {
                default_values.emplace_back(
                    (body_iterator + 1)->lexme,
                    (body_iterator + 3)->lexme
                );
            }
        }

        if(body_iterator->type == TOK_IDENTIFIER && (body_iterator + 1)->type == TOK_LEFT_PAREN) {
            if(body_iterator->lexme != name) {
                throw_err(ERR_CONSTRUCTOR_NAME_DOES_NOT_MATCH_DATA_NAME);
            }
            parsing_constructor = true;
            ++body_iterator;
        }
        if(parsing_constructor && body_iterator->type == TOK_IDENTIFIER) {
            order.emplace_back(body_iterator->lexme);
        }
        if(body_iterator->type == TOK_RIGHT_PAREN) {
            break;
        }

        ++body_iterator;
    }

    DataNode data(is_shared, is_immutable, is_aligned, name, fields, default_values, order);
    return data;
}

/// create_func
///     Creates a FuncNode from the given definition and body tokens.
///     The FuncNode's body is only allowed to house function definitions, and each function has a body respectively.
FuncNode Parser::create_func(const token_list &definition, token_list &body) {
    std::string name;
    std::vector<std::pair<std::string, std::string>> required_data;
    std::vector<FunctionNode> functions;

    auto definition_iterator = definition.begin();
    bool requires_data = false;
    while(definition_iterator != definition.end()) {
        if(definition_iterator->type == TOK_FUNC && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
        }
        if(definition_iterator->type == TOK_REQUIRES) {
            requires_data = true;
        }
        if(requires_data && definition_iterator->type == TOK_IDENTIFIER
            && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            required_data.emplace_back(
                definition_iterator->lexme,
                (definition_iterator + 1)->lexme
            );
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    int current_line = -1;
    while(body_iterator != body.end()) {
        if(current_line == body_iterator->line) {
            ++body_iterator;
            continue;
        }
        current_line = body_iterator->line;

        std::pair<unsigned int, unsigned int> definition_ids = Signature::get_line_token_indices(body, current_line).value();
        token_list function_definition = extract_from_to(definition_ids.first, definition_ids.second, body);

        unsigned int leading_indents = Signature::get_leading_indents(function_definition, current_line).value();
        token_list function_body = get_body_tokens(leading_indents, body);

        functions.emplace_back(create_function(function_definition, function_body));
        ++body_iterator;
    }

    FuncNode func(name, required_data, functions);
    return func;
}

/// create_entity
///     Creates an EntityNode from the given definition and body tokens.
///     An Entity can either be monolithic or modular.
EntityNode Parser::create_entity(const token_list &definition, const token_list &body) {
    return {};
}

/// create_enum
///     Creates an EnumNode from the given definition and body tokens.
EnumNode Parser::create_enum(const token_list &definition, const token_list &body) {
    std::string name;
    std::vector<std::string> values;

    auto definition_iterator = definition.begin();
    while (definition_iterator != definition.end()) {
        if(definition_iterator->type == TOK_ENUM && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
            break;
        }
    }

    auto body_iterator = body.begin();
    while(body_iterator != body.end()) {
        if(body_iterator->type == TOK_IDENTIFIER) {
            if((body_iterator + 1)->type == TOK_COMMA) {
                values.emplace_back(body_iterator->lexme);
            } else if((body_iterator + 1)->type == TOK_SEMICOLON) {
                values.emplace_back(body_iterator->lexme);
                break;
            } else {
                throw_err(ERR_UNEXPECTED_TOKEN);
            }
        }
    }

    EnumNode enum_node(name, values);
    return enum_node;
}

/// create_error
///     Creates an ErrorNode from the given definition and body tokens.
ErrorNode Parser::create_error(const token_list &definition, const token_list &body) {
    return {};
}

/// create_variant
///     Creates a VariantNode from the given definition and body tokens.
VariantNode Parser::create_variant(const token_list &definition, const token_list &body) {
    return {};
}
