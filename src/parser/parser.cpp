#include "parser.hpp"

#include "signature.hpp"

#include "../debug.hpp"
#include "../types.hpp"
#include "../lexer/lexer.hpp"
#include "../error/error.hpp"

#include "ast/node_type.hpp"
#include "ast/program_node.hpp"

#include "ast/definitions/data_node.hpp"
#include "ast/definitions/entity_node.hpp"
#include "ast/definitions/enum_node.hpp"
#include "ast/definitions/error_node.hpp"
#include "ast/definitions/func_node.hpp"
#include "ast/definitions/function_node.hpp"
#include "ast/definitions/import_node.hpp"
#include "ast/definitions/link_node.hpp"
#include "ast/definitions/variant_node.hpp"

#include "ast/expressions/expression_node.hpp"
#include "ast/expressions/literal_node.hpp"
#include "ast/expressions/unary_op_node.hpp"
#include "ast/expressions/variable_node.hpp"

#include "ast/statements/assignment_node.hpp"
#include "ast/statements/declaration_node.hpp"
#include "ast/statements/for_loop_node.hpp"
#include "ast/statements/if_node.hpp"
#include "ast/statements/return_node.hpp"
#include "ast/statements/statement_node.hpp"
#include "ast/statements/while_node.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <vector>
#include <string>
#include <iterator>
#include <utility>
#include <memory>

/// parse_file
///     Parses a file. It will tokenize it using the Lexer and then create the AST of the file and add all the nodes to the passed main ProgramNode
void Parser::parse_file(ProgramNode &program, std::string &file)
{
    token_list tokens = Lexer(file).scan();
    // Consume all tokens and convert them to nodes
    while(!tokens.empty()) {
        add_next_main_node(program, tokens);
    }
    using namespace Debug::AST;
    print_program(program);
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
        default:
        case NodeType::NONE: {
            throw_err(ERR_UNEXPECTED_DEFINITION);
            break;
        }
        case NodeType::IMPORT: {
            if(definition_indentation > 0) {
                throw_err(ERR_USE_STATEMENT_MUST_BE_AT_TOP_LEVEL);
            }
            ImportNode import_node = create_import(definition_tokens);
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
            auto entity_creation = create_entity(definition_tokens, body_tokens);
            program.add_entity(entity_creation.first);
            if(entity_creation.second.has_value()) {
                std::unique_ptr<DataNode> data_node_ptr = std::move(entity_creation.second.value().first);
                std::unique_ptr<FuncNode> func_node_ptr = std::move(entity_creation.second.value().second);
                program.add_data(*data_node_ptr);
                program.add_func(*func_node_ptr);
            }
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
    if (Signature::tokens_contain(tokens, Signature::declaration_explicit)) {
        return NodeType::DECL_EXPLICIT;
    }
    if (Signature::tokens_contain(tokens, Signature::declaration_infered)) {
        return NodeType::DECL_INFERED;
    }
    if (Signature::tokens_contain(tokens, Signature::assignment)) {
        return NodeType::ASSIGNMENT;
    }
    if (Signature::tokens_contain(tokens, Signature::for_loop)) {
        return NodeType::FOR_LOOP;
    }
    if (Signature::tokens_contain(tokens, Signature::enhanced_for_loop)) {
        return NodeType::ENH_FOR_LOOP;
    }
    if (Signature::tokens_contain(tokens, Signature::par_for_loop)) {
        return NodeType::PAR_FOR_LOOP;
    }
    if (Signature::tokens_contain(tokens, Signature::while_loop)) {
        return NodeType::WHILE_LOOP;
    }
    if (Signature::tokens_contain(tokens, Signature::if_statement)) {
        return NodeType::IF;
    }
    if (Signature::tokens_contain(tokens, Signature::else_if_statement)) {
        return NodeType::ELSE_IF;
    }
    if (Signature::tokens_contain(tokens, Signature::else_statement)) {
        return NodeType::ELSE;
    }
    if (Signature::tokens_contain(tokens, Signature::return_statement)) {
        return NodeType::RETURN;
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
    assert(to >= from);
    token_list extraction;
    if(to == from) {
        return extraction;
    }
    extraction.reserve(to - from);
    std::move(tokens.begin() + from, tokens.begin() + to, std::back_inserter(extraction));
    tokens.erase(tokens.begin() + from, tokens.begin() + to);
    return extraction;
}

std::optional<VariableNode> Parser::create_variable(const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_unary_op
///
std::optional<UnaryOpNode> Parser::create_unary_op(const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_literal
///
std::optional<LiteralNode> Parser::create_literal(const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_call
///
std::optional<CallNode> Parser::create_call(const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_binary_op
///
std::optional<BinaryOpNode> Parser::create_binary_op(const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_expression
///
std::optional<std::unique_ptr<ExpressionNode>> Parser::create_expression(const token_list &tokens) {
    // just return an empty expression for now
    // TODO: make the code actually functional.
    return std::make_unique<ExpressionNode>();
    //throw_err(ERR_NOT_IMPLEMENTED_YET);
    //return std::nullopt;
}

/// create_return
///
std::optional<ReturnNode> Parser::create_return(const token_list &tokens) {
    // just return an empty return for now
    // TODO: make the code actually functional.
    auto expr = ExpressionNode();
    return ReturnNode(expr);
    //throw_err(ERR_NOT_IMPLEMENTED_YET);
    //return std::nullopt;
}

/// create_if
///
std::optional<IfNode> Parser::create_if(const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_while_loop
///
std::optional<WhileNode> Parser::create_while_loop(const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_for_loop
///
std::optional<ForLoopNode> Parser::create_for_loop(const token_list &tokens, const bool &is_enhanced) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_assignment
///
std::optional<AssignmentNode> Parser::create_assignment(const token_list &tokens) {
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

/// create_declaration_statement
///
std::optional<DeclarationNode> Parser::create_declaration(token_list &tokens, const bool &is_infered) {
    std::optional<DeclarationNode> declaration = std::nullopt;
    std::string type;
    std::string name;
    std::optional<std::unique_ptr<ExpressionNode>> initializer = std::nullopt;

    if(is_infered) {
        throw_err(ERR_NOT_IMPLEMENTED_YET);
    } else {
        const Signature::signature lhs = Signature::match_until_signature({TOK_EQUAL});
        std::pair<unsigned int, unsigned int> lhs_range = Signature::get_match_ranges(tokens, lhs).at(0);
        token_list lhs_tokens = extract_from_to(lhs_range.first, lhs_range.second, tokens);

        auto iterator = lhs_tokens.begin();
        unsigned int type_begin = 0;
        unsigned int type_end = 0;
        while(iterator != lhs_tokens.end()) {
            if(iterator->type == TOK_INDENT) {
                ++type_begin;
            }
            if((iterator + 1)->type == TOK_IDENTIFIER && (iterator + 2)->type == TOK_EQUAL) {
                const token_list type_tokens = extract_from_to(type_begin, type_end, lhs_tokens);
                type = Lexer::to_string(type_tokens);
                name = (iterator + 1)->lexme;
                break;
            }
            ++type_end;
            ++iterator;
        }

        auto expr = create_expression(lhs_tokens);
        if(expr.has_value()) {
            initializer = std::move(expr.value());
            declaration = DeclarationNode(type, name, initializer.value());
        }
    }

    return declaration;
}

/// create_statement
///     Creates a statement from the given list of tokens
std::optional<std::unique_ptr<StatementNode>> Parser::create_statement(token_list &tokens) {
    NodeType statement_type = what_is_this(tokens);
    std::optional<std::unique_ptr<StatementNode>> statement_node = std::nullopt;

    switch(statement_type) {
        default:
        case NodeType::NONE: {
            throw_err(ERR_UNDEFINED_STATEMENT);
            break;
        }
        case NodeType::DECL_EXPLICIT: {
            std::optional<DeclarationNode> decl = create_declaration(tokens, false);
            if(decl.has_value()) {
                statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
            } else {
                throw_err(ERR_PARSING);
            }
            break;
        }
        case NodeType::DECL_INFERED: {
            std::optional<DeclarationNode> decl = create_declaration(tokens, true);
            if(decl.has_value()) {
                statement_node = std::make_unique<DeclarationNode>(std::move(decl.value()));
            } else {
                throw_err(ERR_PARSING);
            }
            break;
        }
        case NodeType::ASSIGNMENT: {
            std::optional<AssignmentNode> assign = create_assignment(tokens);
            if(assign.has_value()) {
                statement_node = std::make_unique<AssignmentNode>(std::move(assign.value()));
            } else {
                throw_err(ERR_PARSING);
            }
            break;
        }
        case NodeType::FOR_LOOP: {
            std::optional<ForLoopNode> for_loop = create_for_loop(tokens, false);
            if(for_loop.has_value()) {
                statement_node = std::make_unique<ForLoopNode>(std::move(for_loop.value()));
            } else {
                throw_err(ERR_PARSING);
            }
            break;
        }
        case NodeType::PAR_FOR_LOOP:
        case NodeType::ENH_FOR_LOOP: {
            std::optional<ForLoopNode> enh_for_loop = create_for_loop(tokens, true);
            if(enh_for_loop.has_value()) {
                statement_node = std::make_unique<ForLoopNode>(std::move(enh_for_loop.value()));
            } else {
                throw_err(ERR_PARSING);
            }
            break;
        }
        case NodeType::WHILE_LOOP: {
            std::optional<WhileNode> while_loop = create_while_loop(tokens);
            if(while_loop.has_value()) {
                statement_node = std::make_unique<WhileNode>(std::move(while_loop.value()));
            } else {
                throw_err(ERR_PARSING);
            }
            break;
        }
        case NodeType::IF:
        case NodeType::ELSE_IF:
        case NodeType::ELSE: {
            std::optional<IfNode> if_node = create_if(tokens);
            if(if_node.has_value()) {
                statement_node = std::make_unique<IfNode>(std::move(if_node.value()));
            } else {
                throw_err(ERR_PARSING);
            }
            break;
        }
        case NodeType::RETURN: {
            std::optional<ReturnNode> return_node = create_return(tokens);
            if(return_node.has_value()) {
                statement_node = std::make_unique<ReturnNode>(std::move(return_node.value()));
            } else {
                throw_err(ERR_PARSING);
            }
            break;
        }
    }

    return statement_node;
}

/// create_body
///     Creates a body containing of multiple statement nodes from a list of tokens
std::vector<std::unique_ptr<StatementNode>> Parser::create_body(token_list &body) {
    std::vector<std::unique_ptr<StatementNode>> body_statements;

    const Signature::signature statement_signature = Signature::match_until_signature({TOK_SEMICOLON});

    const std::vector<std::pair<unsigned int, unsigned int>> statements = Signature::get_match_ranges(body, statement_signature);

    for(std::pair<unsigned int, unsigned int> statement : statements) {
        token_list statement_tokens = extract_from_to(statement.first, statement.second, body);
        std::optional<std::unique_ptr<StatementNode>> next_statement = create_statement(statement_tokens);
        if(next_statement.has_value()) {
            body_statements.emplace_back(
                std::move(next_statement.value())
            );
        } else {
            throw_err(ERR_UNDEFINED_STATEMENT);
        }
    }

    return body_statements;
}

/// create_function
///     Creates a FunctionNode from the given definiton tokens of the FunctionNode as well as its body. Will cause additional creation of AST Nodes for the body
FunctionNode Parser::create_function(const token_list &definition, token_list &body) {
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
    std::vector<std::unique_ptr<StatementNode>> body_nodes = create_body(body);
    return FunctionNode(is_aligned, is_const, name, parameters, return_types, std::move(create_body(body)));
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

    return DataNode(is_shared, is_immutable, is_aligned, name, fields, default_values, order);
}

/// create_func
///     Creates a FuncNode from the given definition and body tokens.
///     The FuncNode's body is only allowed to house function definitions, and each function has a body respectively.
FuncNode Parser::create_func(const token_list &definition, token_list &body) {
    std::string name;
    std::vector<std::pair<std::string, std::string>> required_data;
    std::vector<std::unique_ptr<FunctionNode>> functions;

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

        functions.emplace_back(std::make_unique<FunctionNode>(std::move(create_function(function_definition, function_body))));
        ++body_iterator;
    }

    return FuncNode(name, required_data, std::move(functions));
}

/// create_entity
///     Creates an EntityNode from the given definition and body tokens.
///     An Entity can either be monolithic or modular.
///     If its modular, only the EntityNode (result.first) will be returned
///     If it is monolithic, the data and func content will be returned within the optional pair
std::pair<EntityNode, std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>>> Parser::create_entity(const token_list &definition, token_list &body) {
    bool is_modular = Signature::tokens_match(body, Signature::entity_body);
    std::string name;
    std::vector<std::string> data_modules;
    std::vector<std::string> func_modules;
    std::vector<std::unique_ptr<LinkNode>> link_nodes;
    std::vector<std::pair<std::string, std::string>> parent_entities;
    std::vector<std::string> constructor_order;
    std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>> monolithic_nodes = std::nullopt;

    auto definition_iterator = definition.begin();
    bool extract_parents = false;
    while (definition_iterator != definition.end()) {
        if(definition_iterator->type == TOK_ENTITY && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
        }
        if(definition_iterator->type == TOK_LEFT_PAREN
            && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
                extract_parents = true;
                ++definition_iterator;
            }
        if(extract_parents && definition_iterator->type == TOK_IDENTIFIER && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            parent_entities.emplace_back(
                definition_iterator->lexme,
                (definition_iterator + 1)->lexme
            );
        }
        ++definition_iterator;
    }

    if(is_modular) {
        bool extracting_data = false;
        bool extracting_func = false;
        auto body_iterator = body.begin();
        while (body_iterator != body.end()) {
            if(body_iterator->type == TOK_DATA) {
                extracting_data = true;
            } else if(body_iterator->type == TOK_FUNC) {
                extracting_func = true;
            } else if(body_iterator->type == TOK_LINK) {
                unsigned int link_indentation = Signature::get_leading_indents(body, body_iterator->line).value();
                // copy all tokens from the body after the link declaration
                token_list tokens_after_link;
                tokens_after_link.reserve(body.size() - std::distance(body.begin(), body_iterator + 2));
                std::copy(body_iterator + 2, body.end(), tokens_after_link.begin());
                token_list link_tokens = get_body_tokens(link_indentation, tokens_after_link);
                link_nodes = create_links(link_tokens);
            }

            if(extracting_data) {
                if(body_iterator->type == TOK_IDENTIFIER) {
                    data_modules.emplace_back(body_iterator->lexme);
                    if((body_iterator + 1)->type == TOK_SEMICOLON) {
                        extracting_data = false;
                    }
                }
            } else if(extracting_func) {
                if(body_iterator->type == TOK_IDENTIFIER) {
                    func_modules.emplace_back(body_iterator->lexme);
                    if((body_iterator + 1)->type == TOK_SEMICOLON) {
                        extracting_func = false;
                    }
                }
            }
            ++body_iterator;
        }
    } else {
        DataNode data_node;
        FuncNode func_node;
        auto body_iterator = body.begin();
        while (body_iterator != body.end()) {
            if(body_iterator->type == TOK_DATA) {
                // TODO: Add a generic constructor for the data module
                unsigned int leading_indents = Signature::get_leading_indents(body, body_iterator->line).value();
                token_list data_body = get_body_tokens(leading_indents, body);
                token_list data_definition = {TokenContext{TOK_DATA, "", 0}, TokenContext{TOK_IDENTIFIER, name + "__D", 0}};
                data_node = create_data(data_definition, data_body);
                data_modules.emplace_back(name + "__D");
            } else if (body_iterator->type == TOK_FUNC) {
                unsigned int leading_indents = Signature::get_leading_indents(body, body_iterator->line).value();
                token_list func_body = get_body_tokens(leading_indents, body);
                token_list func_definition = {
                    TokenContext{TOK_FUNC},
                    TokenContext{TOK_IDENTIFIER, name + "__F"},
                    TokenContext{TOK_REQUIRES},
                    TokenContext{TOK_LEFT_PAREN},
                    TokenContext{TOK_IDENTIFIER, name + "__D"},
                    TokenContext{TOK_IDENTIFIER, "d"},
                    TokenContext{TOK_RIGHT_PAREN},
                    TokenContext{TOK_COLON}
                };
                // TODO: change the functions accesses to the data by placing "d." in front of every variable access.
                func_node = create_func(func_definition, func_body);
                func_modules.emplace_back(name + "__F");
            }
            ++body_iterator;
        }
    }

    std::pair<unsigned int, unsigned int> constructor_token_ids = Signature::get_match_ranges(body, Signature::entity_body_constructor).at(0);
    for(unsigned int i = constructor_token_ids.first; i < constructor_token_ids.second; i++) {
        if(body.at(i).type == TOK_IDENTIFIER) {
            if(body.at(i + 1).type == TOK_LEFT_PAREN
                && body.at(i).lexme != name) {
                throw_err(ERR_ENTITY_CONSTRUCTOR_NAME_DOES_NOT_MATCH_ENTITY_NAME);
            }
            constructor_order.emplace_back(body.at(i).lexme);
        }
    }
    EntityNode entity(name, data_modules, func_modules, std::move(link_nodes), parent_entities, constructor_order);
    std::pair<EntityNode, std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>>> return_value = std::make_pair(std::move(entity), std::move(monolithic_nodes));
    return return_value;
}

/// create_links
///     Creates a list of LinkNode's from a given body containing those links
std::vector<std::unique_ptr<LinkNode>> Parser::create_links(token_list &body) {
    std::vector<std::unique_ptr<LinkNode>> links;

    std::vector<std::pair<unsigned int, unsigned int>> link_matches = Signature::get_match_ranges(body, Signature::entity_body_link);
    links.reserve(link_matches.size());
    for(std::pair<unsigned int, unsigned int> link_match : link_matches) {
        links.emplace_back(
            std::make_unique<LinkNode>(std::move(create_link(body)))
        );
    }

    return links;
}

/// create_link
///     Creates a LinkNode from the given tokens.
LinkNode Parser::create_link(const token_list &tokens) {
    std::vector<std::string> from_references;
    std::vector<std::string> to_references;

    std::vector<std::pair<unsigned int, unsigned int>> references = Signature::get_match_ranges(tokens, Signature::reference);

    for(unsigned int i = references.at(0).first; i < references.at(0).second; i++) {
        if(tokens.at(i).type == TOK_IDENTIFIER) {
            from_references.emplace_back(tokens.at(i).lexme);
        }
    }
    for(unsigned int i = references.at(1).first; i < references.at(1).second; i++) {
        if(tokens.at(i).type == TOK_IDENTIFIER) {
            to_references.emplace_back(tokens.at(i).lexme);
        }
    }

    return LinkNode(from_references, to_references);
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
        ++definition_iterator;
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
        ++body_iterator;
    }

    return EnumNode(name, values);
}

/// create_error
///     Creates an ErrorNode from the given definition and body tokens.
ErrorNode Parser::create_error(const token_list &definition, const token_list &body) {
    std::string name;
    std::string parent_error;
    std::vector<std::string> error_types;

    auto definition_iterator = definition.begin();
    while (definition_iterator != definition.end()) {
        if(definition_iterator->type == TOK_ERROR && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
        }
        if(definition_iterator->type == TOK_LEFT_PAREN) {
            if((definition_iterator + 1)->type == TOK_IDENTIFIER
            && (definition_iterator + 2)->type == TOK_RIGHT_PAREN) {
                parent_error = (definition_iterator + 1)->lexme;
                break;
            }
            throw_err(ERR_CAN_ONLY_EXTEND_FROM_SINGLE_ERROR_SET);
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    while(body_iterator != body.end()) {
        if(body_iterator->type == TOK_IDENTIFIER) {
            if((body_iterator + 1)->type == TOK_COMMA) {
                error_types.emplace_back(body_iterator->lexme);
            } else if((body_iterator + 1)->type == TOK_SEMICOLON) {
                error_types.emplace_back(body_iterator->lexme);
                break;
            } else {
                throw_err(ERR_UNEXPECTED_TOKEN);
            }
        }
        ++body_iterator;
    }

    return ErrorNode(name, parent_error, error_types);
}

/// create_variant
///     Creates a VariantNode from the given definition and body tokens.
VariantNode Parser::create_variant(const token_list &definition, const token_list &body) {
    std::string name;
    std::vector<std::string> possible_types;

    auto definition_iterator = definition.begin();
    while(definition_iterator != definition.end()) {
        if(definition_iterator->type == TOK_VARIANT
            && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
            break;
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    while (body_iterator != body.end()) {
        if(body_iterator->type == TOK_IDENTIFIER) {
            if((body_iterator + 1)->type == TOK_COMMA) {
                possible_types.emplace_back(body_iterator->lexme);
            } else if((body_iterator + 1)->type == TOK_SEMICOLON) {
                possible_types.emplace_back(body_iterator->lexme);
                break;
            } else {
                throw_err(ERR_UNEXPECTED_TOKEN);
            }
        }
        ++body_iterator;
    }

    return VariantNode(name, possible_types);
}

/// create_import
///     Creates an ImportNode from the given token list
ImportNode Parser::create_import(const token_list &tokens) {
    std::variant<std::string, std::vector<std::string>> import_path;

    if(Signature::tokens_contain(tokens, {TOK_STR_VALUE})) {
        for(const auto &tok : tokens) {
            if(tok.type == TOK_STR_VALUE) {
                import_path = "\"" + tok.lexme + "\"";
                break;
            }
        }
    } else {
        const Signature::signature reference = {"((", TOK_FLINT, ")|(", TOK_IDENTIFIER, "))", "(", TOK_DOT, TOK_IDENTIFIER, ")*"};
        const auto matches = Signature::get_match_ranges(tokens, reference).at(0);
        std::vector<std::string> path;
        if(tokens.at(matches.first).type == TOK_FLINT) {
            path.emplace_back("flint");
        }
        for(unsigned int i = matches.first; i < matches.second; i++) {
            if(tokens.at(i).type == TOK_IDENTIFIER) {
                path.emplace_back(tokens.at(i).lexme);
            }
        }
        import_path = path;
    }

    return ImportNode(import_path);
}
