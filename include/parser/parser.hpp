#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include "types.hpp"

#include "ast/file_node.hpp"

#include "ast/definitions/data_node.hpp"
#include "ast/definitions/entity_node.hpp"
#include "ast/definitions/enum_node.hpp"
#include "ast/definitions/error_node.hpp"
#include "ast/definitions/func_node.hpp"
#include "ast/definitions/function_node.hpp"
#include "ast/definitions/import_node.hpp"
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
#include "ast/statements/catch_node.hpp"
#include "ast/statements/throw_node.hpp"

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

    /// get_call_from_id
    ///     Returns the call node with the given id
    static std::optional<CallNode *> get_call_from_id(unsigned int call_id) {
        std::optional<CallNode *> call_node = std::nullopt;
        for (const auto [id, node] : call_nodes) {
            if (id == call_id) {
                call_node = node;
                break;
            }
        }
        return call_node;
    }

    static void resolve_call_types(const std::unordered_map<std::string, FileNode> &file_map);

  private:
    static std::vector<std::pair<unsigned int, CallNode *>> call_nodes;

    /// set_last_parsed_call
    ///     Sets the last parsed call
    static void set_last_parsed_call(unsigned int call_id, CallNode *call) {
        call_nodes.emplace_back(call_id, call);
    }

    /// get_last_parsed_call_id
    ///     Returns the id of the last parsed call
    static unsigned int get_last_parsed_call_id() {
        return call_nodes.back().first;
    }

    // --- UTILITY ---
    static void add_next_main_node(FileNode &file_node, token_list &tokens);
    static token_list get_definition_tokens(token_list &tokens);
    static token_list get_body_tokens(unsigned int definition_indentation, token_list &tokens);
    static token_list extract_from_to(unsigned int from, unsigned int to, token_list &tokens);
    static token_list clone_from_to(unsigned int from, unsigned int to, const token_list &tokens);

    // --- EXPRESSIONS ---
    static std::optional<VariableNode> create_variable(Scope *scope, const token_list &tokens);
    static std::optional<UnaryOpNode> create_unary_op(Scope *scope, const token_list &tokens);
    static std::optional<LiteralNode> create_literal(const token_list &tokens);
    static std::optional<std::unique_ptr<CallNode>> create_call(Scope *scope, token_list &tokens);
    static std::optional<BinaryOpNode> create_binary_op(Scope *scope, token_list &tokens);
    static std::optional<std::unique_ptr<ExpressionNode>> create_expression(Scope *scope, token_list &tokens);

    // --- STATEMENTS ---
    static std::optional<ThrowNode> create_throw(Scope *scope, token_list &tokens);
    static std::optional<ReturnNode> create_return(Scope *scope, token_list &tokens);
    static std::optional<std::unique_ptr<IfNode>> create_if(Scope *scope, std::vector<std::pair<token_list, token_list>> &if_chain);
    static std::optional<std::unique_ptr<WhileNode>> create_while_loop(Scope *scope, const token_list &definition, token_list &body);
    static std::optional<std::unique_ptr<ForLoopNode>> create_for_loop(Scope *scope, const token_list &definition, const token_list &body);
    static std::optional<std::unique_ptr<ForLoopNode>> create_enh_for_loop( //
        Scope *scope,                                                       //
        const token_list &definition,                                       //
        const token_list &body                                              //
    );
    static std::optional<std::unique_ptr<CatchNode>> create_catch( //
        Scope *scope,                                              //
        const token_list &definition,                              //
        token_list &body,                                          //
        std::vector<body_statement> &statements                    //
    );
    static std::optional<std::unique_ptr<AssignmentNode>> create_assignment(Scope *scope, token_list &tokens);
    static std::optional<DeclarationNode> create_declaration(Scope *scope, token_list &tokens, const bool &is_infered);
    static std::optional<std::unique_ptr<StatementNode>> create_statement(Scope *scope, token_list &tokens);
    static std::optional<std::unique_ptr<StatementNode>> create_scoped_statement( //
        Scope *scope,                                                             //
        const token_list &definition,                                             //
        token_list &body,                                                         //
        std::vector<body_statement> &statements                                   //
    );
    static std::vector<body_statement> create_body(Scope *scope, token_list &body);

    // --- DEFINITIONS ---
    static FunctionNode create_function(const token_list &definition, token_list &body);
    static DataNode create_data(const token_list &definition, const token_list &body);
    static FuncNode create_func(const token_list &definition, token_list &body);
    static std::pair<EntityNode, std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>>> create_entity( //
        const token_list &definition,                                                                                           //
        token_list &body                                                                                                        //
    );
    static std::vector<std::unique_ptr<LinkNode>> create_links(token_list &body);
    static LinkNode create_link(const token_list &tokens);
    static EnumNode create_enum(const token_list &definition, const token_list &body);
    static ErrorNode create_error(const token_list &definition, const token_list &body);
    static VariantNode create_variant(const token_list &definition, const token_list &body);
    static ImportNode create_import(const token_list &tokens);
};

#endif
