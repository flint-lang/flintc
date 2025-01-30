#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include "types.hpp"

#include "ast/call_node_base.hpp"
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
#include "ast/statements/call_node_statement.hpp"
#include "ast/statements/declaration_node.hpp"
#include "ast/statements/for_loop_node.hpp"
#include "ast/statements/if_node.hpp"
#include "ast/statements/return_node.hpp"
#include "ast/statements/statement_node.hpp"
#include "ast/statements/while_node.hpp"

#include "ast/expressions/binary_op_node.hpp"
#include "ast/expressions/call_node_expression.hpp"
#include "ast/expressions/expression_node.hpp"
#include "ast/expressions/literal_node.hpp"
#include "ast/expressions/unary_op_node.hpp"
#include "ast/expressions/variable_node.hpp"
#include "ast/statements/catch_node.hpp"
#include "ast/statements/throw_node.hpp"

#include <cassert>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <utility>

/// @class `Parser`
/// @brief The class which is responsible for the AST generation (parsing)
/// @note This class cannot be initialized and all functions within this class are static
class Parser {
  public:
    Parser() = delete;

    /// @function `parse_file`
    /// @brief Parses a file. It will tokenize it using the Lexer and then create the AST of the file and return the parsed FileNode
    ///
    /// @param `file` The path to the file to tokenize and parse
    /// @return `FileNode` The parsed file
    ///
    /// @note This function creates a new Lexer class to tokenize the given file, so no further tokenization has to be made, this function
    /// takes care of the tokenization too
    static FileNode parse_file(const std::filesystem::path &file);

    /// @function `get_call_from_id`
    /// @brief Returns the call node from the given id
    ///
    /// @param `call_id` The id from which the call node will be returned
    /// @return `std::optional<CallNodeBase *>` An optional pointer to the CallNodeBase, nullopt if no call with the given call id exists
    static std::optional<CallNodeBase *> get_call_from_id(unsigned int call_id);

    /// @function `resolve_call_types`
    /// @brief Resolves all types of all calls
    static void resolve_call_types();

  private:
    /// @var `token_precedence`
    /// @brief
    ///
    /// This map exists to map the token types to their respective precedence values. The higher the precedence the sooner this token will
    /// be evaluated in, for example, a binary operation.
    ///
    /// @details
    /// - **Key** `Token` - The enum value of token type whose precedence is set
    /// - **Value** `unsigned int` - The precedence of the token type
    static const inline std::unordered_map<Token, unsigned int> token_precedence = {
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

    /// @var `call_nodes`
    /// @brief Stores all the calls that have been parsed
    ///
    /// This map exists to keep track of all parsed call nodes. This map is not allowed to be changed to an unordered_map, because the
    /// elements of the map are required to perserve their ordering. Most of times only the last element from this map is searched for, this
    /// is the reason to why the ordering is important.
    static std::map<unsigned int, CallNodeBase *> call_nodes;

    /// @function `set_last_parsed_call`
    /// @brief Sets the last parsed call
    ///
    /// @param `call_id` The call ID of the CallNode to add
    /// @param `call` The pointer to the base of the CallNode to add
    static inline void set_last_parsed_call(unsigned int call_id, CallNodeBase *call) {
        call_nodes.emplace(call_id, call);
    }

    /// @function `get_last_parsed_call_id`
    /// @brief Returns the ID of the last parsed call
    ///
    /// @return The ID of the last parsed call
    ///
    /// @attention Asserts that the call_nodes map contains at least one element
    static inline unsigned int get_last_parsed_call_id() {
        assert(!call_nodes.empty());
        return call_nodes.rbegin()->first;
    }

    // @class `Util`
    /// @brief This class is responsible for parsing everything about expressions
    /// @note This class cannot be initialized and all functions within this class are static
    class Util {
      public:
        Util() = delete;

        /// @function `add_next_main_node`
        /// @brief Creates the next main node from the list of tokens and adds it to the file node
        ///
        /// @details A main node is every node which is able to be at the top-level of a file (functions, modules, file imports etc.)
        ///
        /// @param `file_node` The file node the next created main node will be added to
        /// @param `tokens` The list of tokens from which the next main node will be created from
        static void add_next_main_node(FileNode &file_node, token_list &tokens);

        /// @function `get_definition_tokens`
        /// @brief Extracts all the tokens which are part of the definition
        ///
        /// @return `token_list` Returns the extracted tokens, being part of the definition
        ///
        /// @attention Deletes all tokens which are part of the definition from the given tokens list
        static token_list get_definition_tokens(token_list &tokens);

        /// @function `get_body_tokens`
        /// @brief Extracts all the body tokens based on their indentation
        ///
        /// @param `definition_indentation` The indentation level of the definition. The body of the definition will have at least one
        /// indentation level more
        /// @param `tokens` The tokens from which the body will be extracted from
        /// @return `token_list` The extracted list of tokens forming the whole body
        ///
        /// @attention This function modifies the given `tokens` list, the retured tokens are no longer part of the given list
        static token_list get_body_tokens(unsigned int definition_indentation, token_list &tokens);

        /// @function `extract_from_to`
        /// @brief Extracts the tokens from a given index up to the given index from the given tokens list
        ///
        /// @param `from` The start index from which the extraction starts (inclusive)
        /// @param `to` The end index at which the extraction ends (exclusive)
        /// @param `tokens` The tokens from which the range is extracted
        /// @return `token_list` The extracted token range
        ///
        /// @assert to >= from and to <= tokens.size()
        /// @attention Modifies the given tokens list, deletes all extracted tokens from it
        static token_list extract_from_to(unsigned int from, unsigned int to, token_list &tokens);

        /// @function `clone_from_to`
        /// @brief Clones the tokens from a given index up to the given index from the given tokens list
        ///
        /// @param `from` The start index from which the cloning starts (inclusive)
        /// @param `to` The end index at which the cloning ends (exclusive)
        /// @param `tokens` The tokens from which the range is cloned
        /// @return `token_list` The cloned token range
        ///
        /// @assert to >= from and to <= tokens.size()
        static token_list clone_from_to(unsigned int from, unsigned int to, const token_list &tokens);

        /// @function `create_call_base`
        /// @brief Creates the base node for all calls
        ///
        /// @details This function is part of the `Util` class, as both call statements as well as call expressions use this function
        ///
        /// @param `scope` The scope the call happens in
        /// @param `tokens` The tokens which will be interpreted as call
        /// @return A optional value containing a tuple, where the first value is the function name, the second value is a list of all
        /// expressions (the argument expression) of the call and the third value is the call's return type, encoded as a string
        static std::optional<std::tuple<std::string, std::vector<std::unique_ptr<ExpressionNode>>, std::string>> create_call_base( //
            Scope *scope,                                                                                                          //
            token_list &tokens                                                                                                     //
        );
    }; // subclass Util

    /// @class `Expression`
    /// @brief This class is responsible for parsing everything about expressions
    /// @note This class cannot be initialized and all functions within this class are static
    class Expression {
      public:
        Expression() = delete;

        static std::optional<VariableNode> create_variable(Scope *scope, const token_list &tokens);
        static std::optional<UnaryOpNode> create_unary_op(Scope *scope, const token_list &tokens);
        static std::optional<LiteralNode> create_literal(const token_list &tokens);
        static std::unique_ptr<CallNodeExpression> create_call_expression(Scope *scope, token_list &tokens);
        static std::optional<BinaryOpNode> create_binary_op(Scope *scope, token_list &tokens);
        static std::optional<std::unique_ptr<ExpressionNode>> create_expression(Scope *scope, token_list &tokens);
    }; // subclass Expression

    /// @class `Statement`
    /// @brief This class is responsible for parsing everything about statements
    /// @note This class cannot be initialized and all functions within this class are static
    class Statement {
      public:
        Statement() = delete;

        static std::unique_ptr<CallNodeStatement> create_call_statement(Scope *scope, token_list &tokens);
        static std::optional<ThrowNode> create_throw(Scope *scope, token_list &tokens);
        static std::optional<ReturnNode> create_return(Scope *scope, token_list &tokens);
        static std::optional<std::unique_ptr<IfNode>> create_if(Scope *scope, std::vector<std::pair<token_list, token_list>> &if_chain);
        static std::optional<std::unique_ptr<WhileNode>> create_while_loop(Scope *scope, const token_list &definition, token_list &body);
        static std::optional<std::unique_ptr<ForLoopNode>> create_for_loop( //
            Scope *scope,                                                   //
            const token_list &definition,                                   //
            const token_list &body                                          //
        );
        static std::optional<std::unique_ptr<ForLoopNode>> create_enh_for_loop( //
            Scope *scope,                                                       //
            const token_list &definition,                                       //
            const token_list &body                                              //
        );
        static std::optional<std::unique_ptr<CatchNode>> create_catch( //
            Scope *scope,                                              //
            const token_list &definition,                              //
            token_list &body,                                          //
            std::vector<std::unique_ptr<StatementNode>> &statements    //
        );
        static std::optional<std::unique_ptr<AssignmentNode>> create_assignment(Scope *scope, token_list &tokens);
        static std::optional<DeclarationNode> create_declaration(Scope *scope, token_list &tokens, const bool &is_infered);
        static std::optional<std::unique_ptr<StatementNode>> create_statement(Scope *scope, token_list &tokens);
        static std::optional<std::unique_ptr<StatementNode>> create_scoped_statement( //
            Scope *scope,                                                             //
            token_list &definition,                                                   //
            token_list &body,                                                         //
            std::vector<std::unique_ptr<StatementNode>> &statements                   //
        );
        static std::vector<std::unique_ptr<StatementNode>> create_body(Scope *scope, token_list &body);
    }; // subclass Statement

    /// @class `Definition`
    /// @brief This class is responsible for parsing everything about definitions
    /// @note This class cannot be initialized and all functions within this class are static
    class Definition {
      public:
        using create_entity_type = std::pair<EntityNode, std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>>>;

        Definition() = delete;
        static FunctionNode create_function(const token_list &definition, token_list &body);
        static DataNode create_data(const token_list &definition, const token_list &body);
        static FuncNode create_func(const token_list &definition, token_list &body);
        static create_entity_type create_entity( //
            const token_list &definition,        //
            token_list &body                     //
        );
        static std::vector<std::unique_ptr<LinkNode>> create_links(token_list &body);
        static LinkNode create_link(const token_list &tokens);
        static EnumNode create_enum(const token_list &definition, const token_list &body);
        static ErrorNode create_error(const token_list &definition, const token_list &body);
        static VariantNode create_variant(const token_list &definition, const token_list &body);
        static ImportNode create_import(const token_list &tokens);
    }; // subclass Definition
};

#endif
