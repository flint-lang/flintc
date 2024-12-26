#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include "../types.hpp"

#include "ast/definitions/import_node.hpp"
#include "ast/file_node.hpp"

#include "ast/definitions/data_node.hpp"
#include "ast/definitions/entity_node.hpp"
#include "ast/definitions/enum_node.hpp"
#include "ast/definitions/error_node.hpp"
#include "ast/definitions/func_node.hpp"
#include "ast/definitions/function_node.hpp"
#include "ast/definitions/link_node.hpp"
#include "ast/definitions/variant_node.hpp"

#include "ast/statements/assignment_node.hpp"
#include "ast/statements/declaration_node.hpp"
#include "ast/statements/for_loop_node.hpp"
#include "ast/statements/if_node.hpp"
#include "ast/statements/return_node.hpp"
#include "ast/statements/statement_node.hpp"
#include "ast/statements/while_node.hpp"

#include "ast/expressions/binary_op_node.hpp"
#include "ast/expressions/call_node.hpp"
#include "ast/expressions/expression_node.hpp"
#include "ast/expressions/literal_node.hpp"
#include "ast/expressions/unary_op_node.hpp"
#include "ast/expressions/variable_node.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <utility>

static const std::map<Token, unsigned int> token_precedence = {
    {TOK_SQUARE, 8},
    {TOK_MULT, 7},
    {TOK_DIV, 7},
    {TOK_PLUS, 6},
    {TOK_MINUS, 6},
    {TOK_LESS, 5},
    {TOK_GREATER, 5},
    {TOK_LESS_EQUAL, 5},
    {TOK_GREATER_EQUAL, 5},
    {TOK_NOT, 4},
    {TOK_AND, 3},
    {TOK_OR, 2},
    {TOK_EQUAL_EQUAL, 1},
    {TOK_NOT_EQUAL, 1},
    {TOK_EQUAL, 0},
};

class Parser {
  public:
    Parser() = delete;

    static FileNode parse_file(const std::filesystem::path &file);

  private:
    // --- UTILITY ---
    static void add_next_main_node(FileNode &file, token_list &tokens);
    static token_list get_definition_tokens(token_list &tokens);
    static token_list get_body_tokens(unsigned int definition_indentation, token_list &tokens);
    static token_list extract_from_to(unsigned int from, unsigned int to, token_list &tokens);

    // --- EXPRESSIONS ---
    static std::optional<VariableNode> create_variable(const token_list &tokens);
    static std::optional<UnaryOpNode> create_unary_op(const token_list &tokens);
    static std::optional<LiteralNode> create_literal(const token_list &tokens);
    static std::optional<std::unique_ptr<CallNode>> create_call(token_list &tokens);
    static std::optional<BinaryOpNode> create_binary_op(token_list &tokens);
    static std::optional<std::unique_ptr<ExpressionNode>> create_expression(token_list &tokens);

    // --- STATEMENTS ---
    static std::optional<ReturnNode> create_return(token_list &tokens);
    static std::optional<IfNode> create_if(const token_list &tokens);
    static std::optional<WhileNode> create_while_loop(const token_list &tokens);
    static std::optional<ForLoopNode> create_for_loop(const token_list &tokens, const bool &is_enhanced);
    static std::optional<std::unique_ptr<AssignmentNode>> create_assignment(token_list &tokens);
    static std::optional<DeclarationNode> create_declaration(token_list &tokens, const bool &is_infered);
    static std::optional<std::unique_ptr<StatementNode>> create_statement(token_list &tokens);
    static std::vector<body_statement> create_body(token_list &body);

    // --- DEFINITIONS ---
    static FunctionNode create_function(const token_list &definition, token_list &body);
    static DataNode create_data(const token_list &definition, const token_list &body);
    static FuncNode create_func(const token_list &definition, token_list &body);
    static std::pair<EntityNode, std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>>> create_entity(
        const token_list &definition, token_list &body);
    static std::vector<std::unique_ptr<LinkNode>> create_links(token_list &body);
    static LinkNode create_link(const token_list &tokens);
    static EnumNode create_enum(const token_list &definition, const token_list &body);
    static ErrorNode create_error(const token_list &definition, const token_list &body);
    static VariantNode create_variant(const token_list &definition, const token_list &body);
    static ImportNode create_import(const token_list &tokens);
};

#endif
